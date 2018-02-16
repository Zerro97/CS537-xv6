# Scheduler

mainc and mpmain in [kernel/main.c](../kernel/main.c) call ``void scheduler(void)`` ([kernel/proc.c](../kernel/proc.c)). The scheduler will endlessly loop the process table and schedule a process whose state is runnable. The following four line code is very important.

    struct proc *p;

    switchuvm(p);
    p->state = RUNNING;
    swtch(&cpu->scheduler, proc->context);
    switchkvm();

``switchuvm(p)`` resets the page table base register pointing to p's page table. 

    lcr3(PADDR(p->pgdir));  // switch to new address space

``swtch(&cpu->scheduler, proc->context);`` pushes the sechduler's context in its stack, changees the %esp to proc's stack pointer and restores proc's context.

Now the cpu will not execute ``switchkvm()`` as the context has changed. It will run the proc.

The cpu reenters the scheduler when there is an interrupt. Similar to system calls, the interrupt requests have numbers defined in [include/traps.h](../include/traps.h). 

    #define T_IRQ0          32      // IRQ 0 corresponds to int T_IRQ

    #define IRQ_TIMER        0
    #define IRQ_KBD          1
    #define IRQ_COM1         4
    #define IRQ_IDE         14
    #define IRQ_ERROR       19
    #define IRQ_SPURIOUS    31

[What is IRQ?](https://en.wikipedia.org/wiki/Interrupt_request_(PC_architecture))

In ``void trap(struct trapframe *tf)``[kernel/trap.c](../kernel/trap.c), 

    // Force process to give up CPU on clock tick.
    // If interrupts were on while locks held, would need to check nlock.
    if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
      yield();

The call chain is ``yield()``[kernel/proc.c](../kernel/proc.c) -> ``sched()``[kernel/proc.c](../kernel/proc.c) -> ``swtch(&proc->context, cpu->scheduler);``. At this point the cpu goes back to the scheduler and the next step is switchkvm(), witch sets the page table base register to kpgdir.

The struct proc, struct cpu and struct context are defined in [kernel/proc.h](../kernel/proc.h).


## Process State
There are six states one process can be in.  ``enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };``. UNUSED is the initial state. ``static struct proc* allocproc(void)`` changes one UNUSED process to EMBRYO. ``int fork(void)`` calls allocproc and changes the new EMBRYO process to RUNNABLE. ``"void sleep(void *chan, struct spinlock *lk)"`` changes the state to SLEEPING and calls sched() which directs to the trap/scheduler. The trap/scheduler switches the process state between RUNNABLE and RUNNING.

One thing worth noting is there are two types of sleep. One is sleep on time

    int
    sys_sleep(void)
    {
      ...
      acquire(&tickslock);
        ...
        sleep(&ticks, &tickslock);
        ...
      release(&tickslock);
      ...
    }

the first argument of the sleep is a ``void*`` which points to the condition value. Here it is ``ticks``. The ``ticks`` is waken up in the trap when a timer interrupt happens.

    void
    trap(struct trapframe *tf)
    {
      ...
      switch(tf->trapno){
      case T_IRQ0 + IRQ_TIMER:
        if(cpu->id == 0){
          acquire(&tickslock);
          ticks++;
          wakeup(&ticks);
          release(&tickslock);
        }
        ...
        break;

The other type of sleep is ``wait()``.     

    int
    wait(void) {
      ...
      sleep(proc, &ptable.lock);  //DOC: wait-sleep
    }

The condition value is proc, i.e. the current running proc. In [kernel/proc.h](../kernel/proc.h).

    extern struct proc *proc asm("%gs:4");     // cpus[cpunum()].proc

It is waken up in 

    void
    exit(void)
    {
      ...
      wakeup1(proc->parent);
      ...
    }

The wakeup1 function searchs the process table, finds processes with CV ``proc->parent`` and changes their states to RUNNABLE.

The ``exit`` function change the state to ZOMBIE. The ``wait`` function change the ZOMBIE processes to UNUSED.



All the functions mentioned are defined in [kernel/proc.c](../kernel/proc.c). 



## benchmark
| Avg Complete time | Round & Robin | Multi-level Feedback Queue |
| ----------------- | ------------- | -------------------------- |
| workload 1        | 307           | 207                        |
| workload 2        | 3491          | 2021                       |
| workload 3        | 293           | 270                        |
| workload 4        | 105           | 79                         |


* workload 1: 5 dry run loops (100000000 times).
* workload 2: 61 dry run loops (100000000 times).
* workload 3: 10 dry run loops (100000000 times) arrive at first and 10 dry run loops (1000000) arrive 1.6 seconds later.
* workload 4: 3 dry run loops, 3 read processes and 3 sleeping processes.
