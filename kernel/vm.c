#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "spinlock.h"

#define MAX_SHM_PGNUM  (4)

extern char data[];  // defined in data.S


static pde_t *kpgdir;  // for use in scheduler()

struct shared_mem {
  int count_;
  int num_pages_;
  void* phys_addr_[MAX_SHM_PGNUM];
};

// struct spinlock shm_lock;
struct shared_mem shm_tab[MAX_SHM_KEY];

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
}

// Set up CPU's kernel segment descriptors.
// Run once at boot time on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map virtual addresses to linear addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpunum()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);

  // Map cpu, and curproc
  c->gdt[SEG_KCPU] = SEG(STA_W, &c->cpu, 8, 0);

  lgdt(c->gdt, sizeof(c->gdt));
  loadgs(SEG_KCPU << 3);
  
  // Initialize cpu-local storage.
  cpu = c;
  proc = 0;
}

// Return the address of the PTE in page table pgdir
// that corresponds to linear address va.  If create!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int create)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)PTE_ADDR(*pde);
  } else {
    if(!create || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table 
    // entries, if necessary.
    *pde = PADDR(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for linear addresses starting at la that refer to
// physical addresses starting at pa. la and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *la, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;
  
  a = PGROUNDDOWN(la);
  last = PGROUNDDOWN(la + size - 1);
  for(;;){
    /* if (pa == 0) {
      cprintf("map la=%x to pa=0\n", la);
    } */
    /* if ((uint)pte == 0xffc00200) {
      cprintf("!!!!!!!!!!!!!!!!!! map pte=%x, pa=%x\n", pte, pa);
    } */
    pte = walkpgdir(pgdir, a, 1);
    if(pte == 0)
      return -1;
    if(*pte & PTE_P) {
      cprintf("remap: pte = %x, *pte = %x\n", pte, *pte);
      panic("remap");
    }
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// The mappings from logical to linear are one to one (i.e.,
// segmentation doesn't do anything).
// There is one page table per process, plus one that's used
// when a CPU is not running any process (kpgdir).
// A user process uses the same page table as the kernel; the
// page protection bits prevent it from using anything other
// than its memory.
// 
// setupkvm() and exec() set up every page table like this:
//   0..640K          : user memory (text, data, stack, heap)
//   640K..1M         : mapped direct (for IO space)
//   1M..end          : mapped direct (for the kernel's text and data)
//   end..PHYSTOP     : mapped direct (kernel heap and user pages)
//   0xfe000000..0    : mapped direct (devices such as ioapic)
//
// The kernel allocates memory for its heap and for user memory
// between kernend and the end of physical memory (PHYSTOP).
// The virtual address space of each user program includes the kernel
// (which is inaccessible in user mode).  The user program addresses
// range from 0 till 640KB (USERTOP), which where the I/O hole starts
// (both in physical memory and in the kernel's virtual address
// space).
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

// Turn on paging.
void
vmenable(void)
{
  uint cr0;

  switchkvm(); // load kpgdir into cr3
  cr0 = rcr0();
  cr0 |= CR0_PG;
  lcr0(cr0);
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(PADDR(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  pushcli();
  cpu->gdt[SEG_TSS] = SEG16(STS_T32A, &cpu->ts, sizeof(cpu->ts)-1, 0);
  cpu->gdt[SEG_TSS].s = 0;
  cpu->ts.ss0 = SEG_KDATA << 3;
  cpu->ts.esp0 = (uint)proc->kstack + KSTACKSIZE;
  ltr(SEG_TSS << 3);
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");
  lcr3(PADDR(p->pgdir));  // switch to new address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;
  
  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, PADDR(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint)addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, (char*)pa, offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz > proc->shm)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    mappages(pgdir, (char*)a, PGSIZE, PADDR(mem), PTE_W|PTE_U);
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(pte && (*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      kfree((char*)pa);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, proc->shm, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P)
      kfree((char*)PTE_ADDR(pgdir[i]));
  }
  kfree((char*)pgdir);
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void*)i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, PADDR(mem), PTE_W|PTE_U) < 0)
      goto bad;
  }
  return d;

bad:
  freevm(d);
  return 0;
}

// Map user virtual address to kernel physical address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)PTE_ADDR(*pte);
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;
  
  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}


// p3b

void
shm_init()
{
  // initlock(&shm_lock, "shm_lock");
  int i = 0;
  for (i = 0; i < MAX_SHM_KEY; i++) {
    shm_tab[i].count_ = 0;
  }
}

int
shm_key_used(int key, uint mask)
{
  if (key < 0 || MAX_SHM_KEY <= key) {
    return 0;
  }
  return (mask >> key) & 0x1;
}

int
shm_add(int key, int num_pages, void* phys_addr[MAX_SHM_PGNUM])
{
  int i;
  // sanity check
  if (key < 0 || MAX_SHM_KEY <= key ||
      num_pages < 0 || MAX_SHM_PGNUM < num_pages) {
    return -1;
  }
  shm_tab[key].count_ = 1;
  shm_tab[key].num_pages_ = num_pages;
  for (i = 0; i < num_pages; i++) {
    shm_tab[key].phys_addr_[i] = phys_addr[i];
  }
  return 0;
}

void
shm_add_count(uint mask) {
  int key;
  // acquire(&shm_lock);
  for (key = 0; key < MAX_SHM_KEY; key++) {
    if (shm_key_used(key, mask))
      shm_tab[key].count_++;
  }
  // release(&shm_lock);
}

int
deallocshm(pde_t *pgdir, uint old_shm, uint new_shm)
{
  pte_t *pte;
  uint a, pa;

  if (new_shm <= old_shm)
    return old_shm;

  a = (uint)PGROUNDDOWN(new_shm - PGSIZE);
  for(; old_shm <= a; a -= PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(pte && (*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree"); 
      *pte = 0;
    }
  }
  return new_shm;
}

// Allocate shared memory. Similar to allocuvm but the sz pointer goes downward
int
allocshm(pde_t *pgdir, uint old_shm, uint new_shm, uint sz/*proc->sz*/,
    void *phys_addr[MAX_SHM_PGNUM])
{
  char *mem;
  uint a;
  int i;

  if (old_shm & 0xFFF || new_shm & 0xFFF ||
      old_shm > USERTOP || new_shm < sz)
    return 0;

  for(i = 0, a = new_shm; a < old_shm; a += PGSIZE, i++){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocshm out of memory\n");
      deallocshm(pgdir, new_shm, old_shm);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    mappages(pgdir, (char*)a, PGSIZE, PADDR(mem), PTE_W|PTE_U);
    phys_addr[i] = mem;
  }
  return new_shm;
}

// Similar to copyuvm, but the copy of shared mem is shallow
// assume the pgdir_dst does not have shared memory before the copy.
int
copyshm(pde_t *pgdir_src, uint shm, pde_t *pgdir_dst)
{
  pte_t *pte;
  uint pa, i;

  for (i = shm; i < USERTOP; i += PGSIZE) {
    if ((pte = walkpgdir(pgdir_src, (void*)i, 0)) == 0)
      panic("copyshm: pte should exist");
    if (!(*pte & PTE_P))
      panic("copyshm: page not present");
    pa = PTE_ADDR(*pte);
    if (mappages(pgdir_dst, (void*)i, PGSIZE, pa, PTE_W|PTE_U) < 0) {
      deallocshm(pgdir_dst, shm, USERTOP);
      return -1;
    }
  }
  return 0;
}

// similar to allocshm, but do not kalloc new physical pages
int
mapshm(pde_t *pgdir, uint old_shm, uint new_shm, uint sz,
    void **phys_addr)
{
  uint a;
  int i;

  if (old_shm & 0xFFF || new_shm & 0xFFF ||
      old_shm > USERTOP || new_shm < sz)
    return 0;

  for(i = 0, a = new_shm; a < old_shm; a += PGSIZE, i++){
    mappages(pgdir, (char*)a, PGSIZE, PADDR(phys_addr[i]), PTE_W|PTE_U);
  }
  return new_shm;
}

void*
shmgetat(int key, int num_pages)
{
  pde_t *pgdir;
  void *phys_addr[MAX_SHM_PGNUM];
  uint shm;
  int i;
  if (key < 0 || MAX_SHM_KEY <= key ||
      num_pages < 0 || MAX_SHM_PGNUM < num_pages) {
    return (void*)-1;   // invalid arguments
  }
  pgdir = proc->pgdir;
  shm = proc->shm;
  if (proc->shm_key_mask >> key & 1) {
    return proc->shm_va[key];
  }
  // acquire(&shm_lock);
  if (shm_tab[key].count_ == 0) {
    shm = allocshm(pgdir, shm, shm - num_pages * PGSIZE, proc->sz, phys_addr);
    if (shm == 0) {
      return (void*)-1;
    }
    // allocation succeeds
    proc->shm_va[key] = (void*)shm;
    shm_add(key, num_pages, phys_addr);
  } else {
    for (i = 0; i < num_pages; i++) {
      phys_addr[i] = shm_tab[key].phys_addr_[i];
    }
    num_pages = shm_tab[key].num_pages_;
    if ((shm = mapshm(pgdir, shm, shm - num_pages * PGSIZE,
            proc->sz, phys_addr)) == 0) {
      return (void*)-1;
    }
    proc->shm_va[key] = (void*)shm;
    shm_tab[key].count_++;
  }
  // release(&shm_lock);
  proc->shm = shm;
  proc->shm_key_mask |= 1 << key;
  return (void*)shm;
}

int
shm_rm(int key)
{
  int i;
  if (key < 0 || MAX_SHM_KEY <= key) {
    return -1;
  }
  struct shared_mem* shmem = &shm_tab[key];
  for (i = 0; i < shmem->num_pages_; i++) {
    kfree((char*) shmem->phys_addr_[i]);
  }
  shmem->count_ = 0;
  return 0;
}

// similar to freevm
int
shm_release(pde_t *pgdir, uint shm, uint key_mask)
{
  int k;
  deallocshm(pgdir, shm, USERTOP);
  // acquire(&shm_lock);
  for (k = 0; k < MAX_SHM_KEY; k++) {
    if (shm_key_used(k, key_mask)) {
      shm_tab[k].count_--;
      if (shm_tab[k].count_ == 0) {
        shm_rm(k);
      }
    }
  }
  // release(&shm_lock);
  return 0;
}

int
shm_refcount(int key)
{
  int count;
  // acquire(&shm_lock);
  count = (key < 0 || MAX_SHM_KEY <= key) ? -1 : shm_tab[key].count_;
  // release(&shm_lock);
  return count;
}
