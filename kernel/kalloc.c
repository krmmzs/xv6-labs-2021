// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct ref_struct {
    struct spinlock lock;
    int cnt[PHYSTOP / PGSIZE]; // reference count
} ref;

void
kinit()
{
    initlock(&kmem.lock, "kmem");
    initlock(&ref.lock, "ref"); // init ref lock
    freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
    char *p;
    p = (char*)PGROUNDUP((uint64)pa_start);
    for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
        // In kfree(), cnt[] will be decremented by 1, which should be set to 1 first,
        // otherwise it will be decremented to a negative number.
        ref.cnt[(uint64)p / PGSIZE] = 1;
        kfree(p);
    }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{

    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    // check the reference count
    // if it is 0, free the page
    // otherwise, decrease the reference count
    acquire(&ref.lock);
    if(--ref.cnt[(uint64)pa / PGSIZE] == 0) {
        release(&ref.lock);

        struct run *r = (struct run*)pa;

        // Fill with junk to catch dangling refs.
        memset(pa, 1, PGSIZE);

        acquire(&kmem.lock);
        r->next = kmem.freelist;
        kmem.freelist = r;
        release(&kmem.lock);
    } else {
        release(&ref.lock);
    }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist;
    if(r) {
        kmem.freelist = r->next;
        acquire(&ref.lock);
        int index = (uint64)r / PGSIZE;
        ref.cnt[index] = 1;
        release(&ref.lock);
    }
    release(&kmem.lock);

    if(r)
        memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
}

/**
* @brief increase the reference count of the page
* @param pa the physical address of the page
*/
int kaddrefcnt(void* pa)
{
    // copy form kfree()
    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        return -1;

    acquire(&ref.lock);
    int index = (uint64)pa / PGSIZE;
    ref.cnt[index] ++;
    release(&ref.lock);

    return 0;
}

/**
* @brief return the reference count of the page
* @param pa the physical address of the page
* @return the reference count
*/
int
krefcnt(void *pa)
{
    return ref.cnt[(uint64)pa / PGSIZE];
}
