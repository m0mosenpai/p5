#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "vm.h"
#include "stdint.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

extern pte_t* walkpgdir(pde_t *pgdir, const void *va, int alloc);
extern int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);

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
  return myproc()->pid;
}

int
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
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_wmap(void) {
  int anon = MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED;
  int filebacked = MAP_FIXED | MAP_SHARED;

  uint addr;
  int length;
  int flags;
  int fd;

  if (argint(0, (int*)&addr) < 0 ||
    argint(1, (int*)&length) < 0 ||
    argint(2, (int*)&flags)  < 0 ||
    argint(3, (int*)&fd)     < 0
  ) return FAILED;

  if ((length <= 0)                        ||
    (flags != filebacked && flags != anon) ||
    (flags == filebacked && fd < 0)        ||
    (addr % PGSIZE != 0 || (addr < 0x60000000 || addr + length > KERNBASE))
  ) return FAILED;

  struct proc *curproc = myproc();
  struct mmap *p_mmaps = curproc->mmaps;
  struct file *file = 0;

  if (flags == filebacked) {
    file = curproc->ofile[fd];
    if (file == 0 || file->readable == 0)
      return FAILED;
  }

  int i = 0;
  while (p_mmaps[i].addr != -1) { i++; }
  if (i >= MAX_WMMAP_INFO) return FAILED;
  int free = i;

  for (i = 0; i < MAX_WMMAP_INFO; i++) {
    if (p_mmaps[i].addr == -1) continue;
    if ((p_mmaps[i].addr <= addr && addr < p_mmaps[i].addr + p_mmaps[i].length) ||
      (p_mmaps[i].addr < addr + length && addr + length <= p_mmaps[i].addr + p_mmaps[i].length)
    ) return FAILED;
  }

  p_mmaps[free].addr = addr;
  p_mmaps[free].length = length;
  p_mmaps[free].flags = flags;
  p_mmaps[free].file = file != 0 ? filedup(file) : 0;
  return addr;
}

int
sys_wunmap(void) {
  uint addr;

  if (argint(0, (int*)&addr) < 0)
    return FAILED;

  if (addr % PGSIZE != 0 || (addr < 0x60000000 || addr >= KERNBASE))
    return FAILED;

  struct proc *curproc = myproc();
  struct mmap *p_mmaps = curproc->mmaps;

  int i;
  int length;
  struct file *file = 0;
  int entry = -1;

  for (i = 0; i < MAX_WMMAP_INFO; i++) {
    if (p_mmaps[i].addr == addr) {
      length = p_mmaps[i].length;
      file = p_mmaps[i].file;
      entry = i;
      break;
    }
  }
  if (entry == -1)
    return FAILED;

  if (file != 0) file->off = 0;
  pde_t *pgdir = myproc()->pgdir;

  for (i = 0; i < PGROUNDUP(length) / PGSIZE; i++) {
    // va in user va space -> pa -> pa in kernel va space
    pte_t *pte = walkpgdir(pgdir, (void*)(uintptr_t)addr + i*PGSIZE, 0);
    if (pte == 0 || !(*pte & PTE_P)) continue;
    char* phys_addr = P2V((uintptr_t)PTE_ADDR(*pte));
    if (file != 0) filewrite(file, phys_addr, PGSIZE);
    kfree(phys_addr);
    *pte = 0;
  }
  p_mmaps[entry].addr = -1;
  p_mmaps[entry].nloaded = 0;
  if (file > 0) fileclose(file);
  return SUCCESS;
}

int
sys_va2pa(void) {
  uint va;
  uint pa;

  if (argint(0, (int*)&va) < 0)
    return FAILED;

  pde_t *pgdir = myproc()->pgdir;  
  pte_t *pte = walkpgdir(pgdir, (void *)(uintptr_t)va, 0);
  if (!pte || !(*pte & PTE_P))
    return FAILED; 

  pa = PTE_ADDR(*pte); 
  pa |= va & 0xFFF; 
  return pa;
}

int
sys_getwmapinfo(void) {
  struct wmapinfo *wminfo;
  struct mmap *p_mmaps;
  int total_mmaps = 0;
  int i;

  if (argptr(0, (void *)&wminfo, sizeof(struct wmapinfo)) < 0)
    return FAILED;

  if (wminfo == 0)
    return FAILED;

  p_mmaps = myproc()->mmaps;
  for (i = 0; i < MAX_WMMAP_INFO; i++) {
    if (p_mmaps[i].addr != -1){
      wminfo->addr[i] = p_mmaps[i].addr;
      wminfo->length[i] = p_mmaps[i].length;
      wminfo->n_loaded_pages[i] = p_mmaps[i].nloaded;
      total_mmaps++;
    }
  }
  wminfo->total_mmaps = total_mmaps;
  return SUCCESS;
}
