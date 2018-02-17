#include "types.h"
#include "x86.h"
#include "pstat.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "sysfunc.h"

extern int num_syscalls;

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since boot.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// p1b
int 
sys_getnumsyscallp(void) {
  return num_syscalls;
}

// p2b
int
sys_getpinfo(void)
{
  struct pstat* ps;
  if (argptr(0, (char**)&ps, sizeof(struct pstat)) < 0) {
    return -1;
  }
  getpinfo(ps);
  return 0;
}

// p3b
int
sys_shmgetat(void)
{
  int key, num_pages;
  if (argint(0, &key) < 0 || argint(1, &num_pages) < 0) {
    return -1;
  }
  return (int)shmgetat(key, num_pages);
}

int
sys_shm_refcount(void)
{
  int key;
  if (argint(0, &key) < 0) {
    return -1;
  }
  return shm_refcount(key);
}
