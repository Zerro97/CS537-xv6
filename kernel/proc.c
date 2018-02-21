#include "types.h"
#include "pstat.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "thread.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct pgdir_refs {
  void* addr;
  int count;
};

struct {
  struct spinlock lock;
  struct pgdir_refs refs[NPROC];
} pgdir_table;

int tick_quota[] = {5, 5, 10, 20};    // priority: 0,1,2,3

static struct proc *initproc;

int nextpid = 1;

extern struct spinlock tickslock;
extern uint ticks;

extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);


// p4b
// pgdir_refs operations

void
init_ref(void) {
  int i;
  initlock(&pgdir_table.lock, "pgdir_table");
  for (i = 0; i < NPROC; i++) {
    pgdir_table.refs[i].count = 0;
  }
}

int
get_count(void* pgdir)
{
  int i;
  for (i = 0; i < NPROC; i++) {
    if (pgdir_table.refs[i].count > 0 &&
        pgdir_table.refs[i].addr == pgdir) {
      return pgdir_table.refs[i].count;
    }
  }
  return 0;
}

int
get_index(void* pgdir)
{
  int i;
  for (i = 0; i < NPROC; i++) {
    if (pgdir_table.refs[i].count > 0 &&
        pgdir_table.refs[i].addr == pgdir) {
      return i;
    }
  }
  return -1;
}

void
add_ref(void* pgdir)
{
  int i;
  i = get_index(pgdir);
  if (i < 0) {
    for (i = 0; i < NPROC; i++) {
      if (pgdir_table.refs[i].count == 0) {
        pgdir_table.refs[i].count = 1;
        pgdir_table.refs[i].addr = pgdir;
        break;
      }
    }
  } else {
    pgdir_table.refs[i].count++;
  }
}

// return the count after delete, -1 means failure
int
delete_ref(void* pgdir)
{
  int i;
  for (i = 0; i < NPROC; i++) {
    if (pgdir_table.refs[i].addr == pgdir &&
        pgdir_table.refs[i].count > 0) {
      pgdir_table.refs[i].count--;
      return pgdir_table.refs[i].count;
    }
  }
  return -1;
}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  init_ref();
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
  int priori;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->shm = USERTOP;
  p->shm_key_mask = 0;
  p->priority = 0;
  for (priori = 0; priori < NPRIOR; priori++) {
    p->ticks_used[priori] = 0;
  }
  
  release(&ptable.lock);

  // Allocate kernel stack if possible.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  acquire(&ptable.lock);
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  // count pgdir
  add_ref((void*)p->pgdir);
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if ((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0
      || copyshm(proc->pgdir, proc->shm, np->pgdir) < 0) {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  // add reference counts for shared mem
  shm_add_count(proc->shm_key_mask);

  // proc struct shm record
  np->shm = proc->shm;
  np->shm_key_mask = proc->shm_key_mask;
  for (i = 0; i < MAX_SHM_KEY; i++) {
    if (shm_key_used(i, np->shm_key_mask))
      np->shm_va[i] = proc->shm_va[i];
  }
  
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;

  // p4b count pgdir
  add_ref((void*)np->pgdir);
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  int pt_count;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      if (p->pgdir == proc->pgdir)     // child thread
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        pt_count = delete_ref(p->pgdir);

        if (pt_count < 0) {
          cprintf("ref count of pgdir=%x is negative\n", p->pgdir);
          panic("pgdir ref");
        } else if (pt_count == 0) {  // last thread
          // Release shared memory
          shm_release(p->pgdir, p->shm, p->shm_key_mask); 
          p->shm = USERTOP;
          p->shm_key_mask = 0;
          freevm(p->pgdir);
        }

        p->state = UNUSED;
        p->ustack = 0;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  int priority = 0;
  int ticks_copy = 0;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    acquire(&tickslock);
    ticks_copy = ticks;
    release(&tickslock);
    
    acquire(&ptable.lock);

    // boost the priority after long time starvation
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->state == RUNNABLE && 0 < p->priority &&
          p->last_sched_time + TSTARV < ticks_copy) {
        p->priority--;
        // for debug purpose
        /* cprintf("boost the priority of pid=%d, new priority=%d, current ticks=%d, last sched=%d\n",
            p->pid, p->priority, ticks_copy, p->last_sched_time); */
      }
    }

    // Loop over process table looking for process to run.
    priority = 0;
    while (priority < NPRIOR) {
    // for (priority = 0; priority < NPRIOR; priority++) {
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if(p->state != RUNNABLE || p->priority != priority)
          continue;

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        proc = p;
        switchuvm(p);
        p->state = RUNNING;
        p->last_sched_time = ticks_copy;
        /* if (p->priority == 3) {
          cprintf("schedule pid=%d at %d\n", p->pid, ticks_copy);
        } */
        swtch(&cpu->scheduler, proc->context);
        switchkvm();
        
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        proc = 0;
        goto newround;
      }
      priority++;
    }
newround:
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// p2b update ticks and priority. The function itself
// does not enforce lock, so it should be inserted in
// a locked region.
void
update_ticks(void) {
  proc->ticks_used[proc->priority]++;
  proc->ticks++;
  if (proc->priority < NPRIOR - 1 &&
      proc->ticks >= tick_quota[proc->priority]) {
    proc->priority++;
    proc->ticks = 0;
    // cprintf("update ticks:pid=%d, priority=%d, ticks=%d\n", proc->pid, proc->priority, proc->ticks);
  }
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  update_ticks();
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// P2b
void
getpinfo(struct pstat* ps) {
  int i = 0;
  int pi = 0;
  struct proc* pptr;
  for (i = 0; i < NPROC; i++) {
    pptr = &ptable.proc[i];
    ps->inuse[i] = (pptr->state != UNUSED);
    ps->pid[i] = pptr->pid;
    ps->priority[i] = pptr->priority;
    ps->state[i] = pptr->state;
    for (pi = 0; pi < pptr->priority; pi++) {
      ps->ticks[i][pi] = tick_quota[pi];
    }
    ps->ticks[i][pptr->priority] = pptr->ticks_used[pptr->priority];
    for (pi = pptr->priority + 1; pi < NPRIOR; pi++) {
      ps->ticks[i][pi] = 0;
    }
  }
}


// p4b
int
clone(thread_func fcn, void *arg, void *stack)
{
  int i, pid;
  uint sp, ustack[2];
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p
  np->pgdir = proc->pgdir;
  
  // proc struct shm record
  np->shm = proc->shm;
  np->shm_key_mask = proc->shm_key_mask;
  for (i = 0; i < MAX_SHM_KEY; i++) {
    if (shm_key_used(i, np->shm_key_mask))
      np->shm_va[i] = proc->shm_va[i];
  }
  
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;
  // p4b count pgdir
  add_ref((void*) np->pgdir);
  safestrcpy(np->name, proc->name, sizeof(proc->name));

  // initialize the stack
  sp = (uint)stack + PGSIZE - 8U;
  ustack[1] = (uint)arg;
  ustack[0] = 0xffffffff;
  if (copyout(np->pgdir, sp, (void*)ustack, sizeof(uint) * 2) < 0) {
    cprintf("clone: copy stack failed\n");
    return -1;
  }
  np->ustack = (char*)stack;
  np->tf->esp = sp;
  np->tf->eip = (uint)fcn;

  return pid;
}

int
join(void **stack)
{
  struct proc *p;
  int havekids, pid;
  int pt_count;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      if (p->pgdir != proc->pgdir)     // child process
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        // get the user stack
        *stack = (void*)p->ustack;
        // Release shared memory
        pt_count = delete_ref(p->pgdir);
        if (pt_count < 0) {
          cprintf("ref count of pgdir=%x is negative\n", p->pgdir);
          panic("pgdir ref");
        } else if (pt_count == 0) {  // last thread
          shm_release(p->pgdir, p->shm, p->shm_key_mask); 
          p->shm = USERTOP;
          p->shm_key_mask = 0;
          freevm(p->pgdir);
        }

        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }

}
