#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// sys_trace() function
uint64 sys_trace(void) {
    int mask;
    // Obtain the mask parameter by reading the trapframe of the process
    if (argint(0, &mask) < 0)
        return -1;
    // Set the trace mask of the process to the mask parameter
    myproc()->trace_mask = mask;
    return 0;
}

uint64 sys_sysinfo(void) {
    // Obtain the dst(user) virtual address
    uint64 sysinfop;
    // get address from a0
    if (argaddr(0, &sysinfop) < 0) {
        return -1;
    }
    // Obtain the info parameter by reading the trapframe of the process
    struct sysinfo sinfo;
    sinfo.freemem = get_freemem(); // kalloc.c
    sinfo.nproc = get_nproc(); // proc.c
    // copy out the sysinfo struct sinfo from kernel space to user space
    if(copyout(myproc()->pagetable, sysinfop, (char *)&sinfo, sizeof(sinfo)) < 0)
      return -1;
    return 0;
}
