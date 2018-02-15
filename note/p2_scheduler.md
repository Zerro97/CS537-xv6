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
