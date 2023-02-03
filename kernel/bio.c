// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define HASH(dev, blockno) (dev + blockno) % NBUCKET

uint extern ticks; // in trap.c

struct {
    struct buf buf[NBUF]; // static memory
    struct spinlock eviction_lock; // spinlock for eviction

    struct spinlock locks[NBUCKET]; // spinlocks for each bucket
    struct buf heads[NBUCKET]; // buffer as heads
} bcache;

void
binit(void)
{
    struct buf *b;
    int i;

    // initialize the eviction lock
    initlock(&bcache.eviction_lock, "eviction");

    // init hash table's locks
    for (i = 0; i < NBUCKET; i ++) {
        initlock(&bcache.locks[i], "bcache");
        bcache.heads[i].next = 0;
    }

    // Initialize buffer cache.
    for (i = 0; i < NBUF; i ++) {
        b = &bcache.buf[i];
        initsleeplock(&b->lock, "buffer");
        b->timestamp = 0;
        b->refcnt = 0;
        // head-in inseart all of the buffers into the first bucket
        b->next = bcache.heads[0].next;
        bcache.heads[0].next = b;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
    struct buf *b;

    uint id = HASH(dev, blockno);
    acquire(&bcache.locks[id]);

    // Is the block already cached?
    b = bcache.heads[id].next;
    while (b) {
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.locks[id]);
            acquiresleep(&b->lock);
            return b;
        }
        b = b->next;
    }

    // Not cached.
    // Check again, is the block already cached?
    // no other eviction & reuse will happen while we are holding eviction_lock,
    // which means no link list structure of any bucket can change.
    // so it's ok here to iterate through `bcache.bufmap[key]` without holding
    // it's cooresponding bucket lock, since we are holding a much stronger eviction_lock.
    release(&bcache.locks[id]);
    acquire(&bcache.eviction_lock);

    b = bcache.heads[id].next;
    while (b) {
        if (b->dev == dev && b->blockno == blockno) {
            acquire(&bcache.locks[id]); // must do, for b->refcnt ++
            b->refcnt++;
            release(&bcache.locks[id]);
            release(&bcache.eviction_lock);
            acquiresleep(&b->lock);
            return b;
        }
        b = b->next;
    }

    // Still not cached.
    // we are now only holding eviction lock, none of the bucket locks are held by us.
    // so it's now safe to acquire any bucket's lock without risking circular wait and deadlock.

    // find the one least-recently-used buf among all buckets.
    // finish with it's corresponding bucket's lock held.
    struct buf *lru = 0;
    // the new block might hash to the same bucket as the old block,
    // so need the fileds to record the bucket we are holding
    int holding_bucket = -1; 
    for (int i = 0; i < NBUCKET; i ++) {
        acquire(&bcache.locks[i]);
        int new_found = 0; // new lru found in this bucket
        for (b = &bcache.heads[i]; b->next; b = b->next) {
            if (b->next->refcnt == 0 && (!lru || b->next->timestamp < lru->next->timestamp)) {
                lru = b;
                new_found = 1;
            }
        }
        if (!new_found) {
            release(&bcache.locks[i]);
        } else {
            if (holding_bucket != -1) {
                release(&bcache.locks[holding_bucket]);
            }
            holding_bucket = i;
            // keep holding this bucket's lock....
        }
    }
    if (!lru) {
        panic("bget: no buffers");
    }
    b = lru->next;
    // move a struct buf from one bucket to another bucket, 
    // holding_bucket to id
    if (holding_bucket != id) {
        // remove the buf from it's original bucket
        lru->next = b->next;

        release(&bcache.locks[holding_bucket]);

        // rehash and add it to the target bucket
        acquire(&bcache.locks[id]);
        b->next = bcache.heads[id].next;
        bcache.heads[id].next = b;
    }

    b->dev = dev;
    b->blockno = blockno;
    b->refcnt = 1;
    b->valid = 0;
    release(&bcache.locks[id]);
    release(&bcache.eviction_lock);
    acquiresleep(&b->lock);
    return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
    struct buf *b;

    b = bget(dev, blockno);
    if(!b->valid) {
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
    if(!holdingsleep(&b->lock))
        panic("bwrite");
    virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
    if(!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);

    uint id = HASH(b->dev, b->blockno);

    acquire(&bcache.locks[id]);
    b->refcnt--;
    if (b->refcnt == 0) {
        // no one is waiting for it.
        b->timestamp = ticks;
    }

    release(&bcache.locks[id]);
}

void
bpin(struct buf *b) {
    uint id = HASH(b->dev, b->blockno);

    acquire(&bcache.locks[id]);
    b->refcnt++;
    release(&bcache.locks[id]);
}

void
bunpin(struct buf *b) {
    uint id = HASH(b->dev, b->blockno);

    acquire(&bcache.locks[id]);
    b->refcnt--;
    release(&bcache.locks[id]);
}


