# Shared memory


## Physical Memory (PM) in xv6 ([kernel/kalloc.c](../kernel/kalloc.c))
The physical memory layout


<pre>
                        <---- PHYTOP
free mem                <---- PGROUNDUP(elf end)
kernel elf file (code)  <---- 0x0
</pre>

The physical memory is organized as a list of free pages.

    struct {
      struct spinlock lock;
      struct run *freelist;
    } kmem;

The first 4 bytes of a page are used to store the address of the next free page.

<pre>
    +-----------------+ <--- PHYSTOP (0x1000000)
    |                 |
    |                 |
    +-----------------+
    | next = 0        |
    +-----------------+ <--+ (0xFFFF000)
    |                 |    |
    |                 |    |
    +-----------------+    |
    | next = 0xFFFF000| ---+
    +-----------------+
    |       ...       |
    +-----------------+ <--+ (0x131000)
    |                 |    |
    |                 |    |
    +-----------------+    |
    | next = 0x131000 | ---+ 
    +-----------------+ <--- freelist = 0x130000
</pre>

``void kinit(void)`` free all pages above the end of the code section.

The free function is ``void kfree(char *v)``. After some sanity checks, it overwrites the whole page with 1's and insert the page pointer ``v`` into the front of ``freelist``.

The allocation function is ``char* kalloc(void)``. It pops the front of ``freelist`` and returns it.


## Virtual Memory (VM) in xv6
The VM layout,
<pre>
          <---- PHYSTOP
kernel    <---- USERTOP
heap
stack     <---- PGGROUPDUP(ELF end), one page
ELF       <---- 0
</pre>
[kernel/mmu.h](../kernel/mmu.h) provides much information about the structure of a page table.
```
 A linear address 'la' has a three-part structure as follows:

 +--------10------+-------10-------+---------12----------+
 | Page Directory |   Page Table   | Offset within Page  |
 |      Index     |      Index     |                     |
 +----------------+----------------+---------------------+
  \--- PDX(la) --/ \--- PTX(la) --/
```

The page table is two-level, meaning that there the ``*pgdir`` points to a ``pde_t`` (page directory entry), whose entries then point to ``pte_t`` (page table entry).

The ``pde_t`` and ``pte_t`` are both unsigned 32-bit int

// Page table/directory entry flags.

    #define PTE_P		0x001	// Present
    #define PTE_W		0x002	// Writeable
    #define PTE_U		0x004	// User
    #define PTE_PWT		0x008	// Write-Through
    #define PTE_PCD		0x010	// Cache-Disable
    #define PTE_A		0x020	// Accessed
    #define PTE_D		0x040	// Dirty
    #define PTE_PS		0x080	// Page Size
    #define PTE_MBZ		0x180	// Bits must be zero

Visualize the pde and pte.

    | PHY_ADDR (20 bits) | 5 bits | PTE_D | PTE_A | PTE_PCD | PTE_PWT | PTE_U | PTE_W | PTE_P

The PHY\_ADDR is the 20 highest bits. Notice that the lowest 12 bits of a page entry is always 0. We can get the PM by 

    #define PTE_ADDR(pte)	((uint)(pte) & ~0xFFF)



The kernel address space is defined in

    static struct kmap {
      void *p;
      void *e;
      int perm;
    } kmap[] = {
      {(void*)USERTOP,    (void*)0x100000, PTE_W},  // I/O space
      {(void*)0x100000,   data,            0    },  // kernel text, rodata
      {data,              (void*)PHYSTOP,  PTE_W},  // kernel data, memory
      {(void*)0xFE000000, 0,               PTE_W},  // device mappings
    };

where 

    #define USERTOP  0xA0000 // end of user address space
    #define PHYSTOP  0x1000000 // use phys mem up to here as free pool

In fact, the PM and VM in kernel space are identical. Thus xv6 maps VM from 0xA0000 to 0x1000000 to PM 0xA0000 to 0x1000000, leaving \[0, 0xA0000) unmapped. Also the kernel space are shared by all user processes.

The kernel has one page table ``static pde_t *kpgdir;``([kernel/vm.c](../kernel/vm.c)). Each process has one page table [kernel/proc.h](..kernel/proc.h). All page tables share the same kernel map (identical, linear). However, the kpgdir does not configure the user space, and different processes maintain different page tables in the user space.

## Connect PM and VM
The routine ``int exec(char *path, char **argv)`` [kernel/exec.c](../kernel/exec.c) initialize the address space for a new process. The first thing it does is set the kernel vm.

    if((pgdir = setupkvm()) == 0)
      goto bad;

``pde_t* setupkvm(void)`` is defined in [kernel/vm.c](../kernel/vm.c).


    // Set up kernel part of a page table.
    pde_t*
    setupkvm(void)
    {
      pde_t *pgdir;
      struct kmap *k;

      if((pgdir = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pgdir, 0, PGSIZE);
      k = kmap;
      for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
        if(mappages(pgdir, k->p, k->e - k->p, (uint)k->p, k->perm) < 0)
          return 0;

      return pgdir;
    }

It calls ``char* kalloc(void) `` ([kernel/kalloc.c](../kernel/kalloc.c)) to get a page (PM) to store the page table (1024 entries). Then it maps the kernel space to PM. 

``static int mappages(pde_t *pgdir, void *la, uint size, uint pa, int perm)`` first aligns the memory segment [la, la + size). 

    a = PGROUNDDOWN(la);
    last = PGROUNDDOWN(la + size - 1);

Then for each page, it walks through the pgdir 

    pte = walkpgdir(pgdir, a, 1);

The ``static pte_t * walkpgdir(pde_t *pgdir, const void *va, int create)`` has many details on the VM operation.

    pde = &pgdir[PDX(va)];
  
gets the page directory entry of ``va``(virtual address). The PDX extracts the first 10 bits of va as the index.

    #define PDX(la)		(((uint)(la) >> PDXSHIFT) & 0x3FF)
    #define PDXSHIFT	22		// offset of PDX in a linear address

If the ``pde`` is valid, it gets the page table (``pgtab``)'s PM from the pde entry. Otherwise it creates a new one. Finally the page table entry with the right index is returned.

    return &pgtab[PTX(va)];

<pre>
  va = | 0x2 | 0x1 | ...
----------------------------------------------------------------------
pgdir +-----------+             +--> pgtab = PA +-----------+
      |           |  pde[0]     |               |           | pte[0]
      +-----------+             |               +-----------+    
      |           |  pde[1]     |               |PA | FLAGS | pte[1]
      +-----------+             |               +-----------+
      |PA  | FLAGS|  pde[2] ----+               |           | pte[2]
      +-----------+                             +-----------+
        ...
</pre>

The next thing is filling out the page table entry.

    *pte = pa | perm | PTE_P;
    ...
    a += PGSIZE;
    pa += PGSIZE;

Return to ``exec``.

    sz = PGROUNDUP(sz);
    if((sz = allocuvm(pgdir, sz, sz + PGSIZE)) == 0)
      goto bad;
    sp = sz;

allocates one page for stack. The ``sp`` stack pointer now points to the top of the stack.

<pre>
    +--------------+ <--- sp
    |              |
    +--------------+
</pre>

Then the arguments are pushed into the stack. After that the stack layout will be

<pre>
    +--------------------+ 
    | argv[0]            |
    |                    |
    +--------------------+ <-- ustack[3]
    | argv[1]            |
    |                    |
    +--------------------+ <-- ustack[4]
    | ...                |
    |                    |
    +--------------------+
    | argv[argc-1]       |
    |                    |
    +--------------------+ <-- ustack[argc+2]
    | ustack[3+argc] = 0 |
    +--------------------+
    | ...                |
    +--------------------+
    | ustack[3]          |
    +--------------------+ <-- ustack[2] = **argv
    | ustack[2]          |
    +--------------------+
    | ustack[1]=argc     |
    +--------------------+
    |ustack[0]=0xffffffff|
    +--------------------+ <-- sp
</pre>

Finally, it modified the ``struct proc``, setting the page table, size, EIP, ESP to the new program and release the old page table.

    oldpgdir = proc->pgdir;
    proc->pgdir = pgdir;
    proc->sz = sz;
    proc->tf->eip = elf.entry;  // main
    proc->tf->esp = sp;
    switchuvm(proc);

    switchuvm(proc);
    freevm(oldpgdir);


## About the project
system call ``void *shmgetat(int key, int num_pages)``

* interface: if processes call shmgetat() with the same key for the first argument, then they will share the specified number of physical pages
* implementation: shmgetat should map the shared phyiscal pages to the next available virtual pages, starting at the high end of that process' address space. 

## Miscs
### Page alignment
* ``#define PGROUNDUP(sz) (((sz)+PGSIZE-1) & ~(PGSIZE-1))``

As PGSIZE is a power of 2. Without loss of the generality assume PGSIZE = 0x00001000. Then ~(PGSIZE-1) = ~(0x00000FFF) = 0xFFFFF000. The effect of PGROUNDUP is to return the smallest number that is greater or equal than sz and has last 12 bits all zeros.

* ``#define PGROUNDDOWN(a) ((char*)((((unsigned int)(a)) & ~(PGSIZE-1))))``

Similar to PGROUNDUP, this function just set the lowest 12 bits zeros.

