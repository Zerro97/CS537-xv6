# About System Calls

System calls connect users and the kernel. From the user's perspective, it is a function declared in [user/user.h](../user/user.h) without implementation in user/ directory. Instead, these system calls will trigger an interrupt once called ([user/usys.S](../user/usys.S)).


    #define SYSCALL(name) \
      .globl name; \
      name: \
        movl $SYS_ ## name, %eax; \
        int $T_SYSCALL; \
        ret

The ${name} is the system call's name. It will move its number (``$SYS\_ ## name``) to the %eax register and issue an interupt with the number $T\_SYSCALL = 64. The system call numbers are defined in [include/syscall.h](../include/syscall.h).

---

On the kernel's side, a trap table is defined in _kernel/vectors.S_. 

    ...
    .globl vector64
    vector64:
      pushl $0
      pushl $64
      jmp alltraps
    ...

The alltraps is defined in [kernel/trapasm.S](../kernel/trapasm.S), which calls trap (in [kernel/trap.c](../kernel/trap.c))


    if(tf->trapno == T_SYSCALL){
        if(proc->killed)
          exit();
        proc->tf = tf;
        syscall();
        if(proc->killed)
          exit();
        return;
      }

``tf`` is of type ``struct trapframe`` ([include/x86.h](../include/x86.h)). If it's trap number is T\_SYSCALL(=64). It will finally reach ``syscall`` [kernel/syscall.c](../kernel/syscall.c).


    void
    syscall(void)
    {
      int num;
      
      num = proc->tf->eax;
      if(num > 0 && num < NELEM(syscalls) && syscalls[num] != NULL) {
        proc->tf->eax = syscalls[num]();
      } else {
        cprintf("%d %s: unknown sys call %d\n",
                proc->pid, proc->name, num);
        proc->tf->eax = -1;
      }
    }

The syscall will get its number from the %eax register and call the function syscalls\[num\](). The syscalls is an array of functions (the initialization of the array uses [Designated Initializers](https://gcc.gnu.org/onlinedocs/gcc-4.0.4/gcc/Designated-Inits.html)) defined just above the syscall function and declared in [kernel/sysfunc.h](../kernel/sysfunc.h). The system call will put the return value in %eax.


## Parameters
The parameters of a system call, if given, are still pushed into the user stack. The kernel can locate the stack by ``proc->tf->esp``. Check ``argint, argptr, argstr`` in [kernel/syscall.c](../kernel/syscall.c).
