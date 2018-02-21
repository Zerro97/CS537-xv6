## xv6 for UW-Madison CS537 (Introduction to Operating Systems)

The repo consists of five projects for UW-Madison C537 Fall 2016. 

1. [P1: add a system call](http://pages.cs.wisc.edu/~dusseau/Classes/CS537/Fall2016/Projects/P1/p1b.html) | [Note](note/p1_syscall.md) Files: [kernel/sysproc.c](kernel/sysproc.c), [kernel/syscall.c](kernel/syscall.c)
1. [P2: multi-level feedback queue scheduler](http://pages.cs.wisc.edu/~dusseau/Classes/CS537/Fall2016/Projects/P2/p2b.html) | [Note](note/p2_scheduler.md) | Files: [kernel/proc.c](kernel/proc.c)
1. [P3: shared memory](http://pages.cs.wisc.edu/~dusseau/Classes/CS537/Fall2016/Projects/P3/p3b.html) | [Note](note/p3_shared_memory.md) | Files: [kernel/vm.c](kernel/proc.c)
1. [P4: threads](http://pages.cs.wisc.edu/~dusseau/Classes/CS537/Fall2016/Projects/P4/p4b.html) | [Note](note/p4_thread.md) | Files: [kernel/proc.c](kernel/proc.c), [user/ulib.c](user/ulib.c)
1. [P5] 



## Original README

xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix
Version 6 (v6).  xv6 loosely follows the structure and style of v6,
but is implemented for a modern x86-based multiprocessor using ANSI C.

ACKNOWLEDGMENTS

xv6 is inspired by John Lions's Commentary on UNIX 6th Edition (Peer
to Peer Communications; ISBN: 1-57398-013-7; 1st edition (June 14,
2000)). See also http://pdos.csail.mit.edu/6.828/2007/v6.html, which
provides pointers to on-line resources for v6.

xv6 borrows code from the following sources:
    JOS (asm.h, elf.h, mmu.h, bootasm.S, ide.c, console.c, and others)
    Plan 9 (bootother.S, mp.h, mp.c, lapic.c)
    FreeBSD (ioapic.c)
    NetBSD (console.c)

The following people made contributions:
    Russ Cox (context switching, locking)
    Cliff Frey (MP)
    Xiao Yu (MP)
    Nickolai Zeldovich
    Austin Clements

In addition, we are grateful for the patches contributed by Greg
Price, Yandong Mao, and Hitoshi Mitake.

The code in the files that constitute xv6 is
Copyright 2006-2007 Frans Kaashoek, Robert Morris, and Russ Cox.

ERROR REPORTS

If you spot errors or have suggestions for improvement, please send
email to Frans Kaashoek and Robert Morris (kaashoek,rtm@csail.mit.edu). 

BUILDING AND RUNNING XV6

To build xv6 on an x86 ELF machine (like Linux or FreeBSD), run "make".
On non-x86 or non-ELF machines (like OS X, even on x86), you will
need to install a cross-compiler gcc suite capable of producing x86 ELF
binaries.  See http://pdos.csail.mit.edu/6.828/2007/tools.html.
Then run "make TOOLPREFIX=i386-jos-elf-".

To run xv6, you can use Bochs or QEMU, both PC simulators.
Bochs makes debugging easier, but QEMU is much faster. 
To run in Bochs, run "make bochs" and then type "c" at the bochs prompt.
To run in QEMU, run "make qemu".

