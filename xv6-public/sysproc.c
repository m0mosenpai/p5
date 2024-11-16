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
#include <stdint.h>

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
  uint addr;
  int length;
  int flags;
  int fd;

  if (argint(0, (int*)&addr) < 0 ||
    argint(1, (int*)&length) < 0 ||
    argint(2, (int*)&flags)  < 0 ||
    argint(3, (int*)&fd)     < 0
  ) return FAILED;

  // length should be positive,
  // flags should always be set to MAP_FIXED and MAP_SHARED
  // fd should be valid if MAP_ANONYMOUS is not set
  // addr should be valid and page divisible
  if ((length <= 0)                                      ||
    (flags & MAP_FIXED) != MAP_FIXED                     ||
    (flags & MAP_SHARED) != MAP_SHARED                   ||
    (flags & MAP_ANONYMOUS) != (MAP_ANONYMOUS && fd < 0) ||
    (addr % PGSIZE != 0 || (addr < 0x60000000 || addr + length > KERNBASE))
  ) return FAILED;

  struct proc *curproc = myproc();
  struct mmap *p_mmaps = curproc->mmaps;

  int i = 0;
  while (p_mmaps[i].valid == 1) { i++; }
  if (i >= MAX_WMMAP_INFO) return FAILED;
  int free = i;

  for (i = 0; i < MAX_WMMAP_INFO; i++) {
    if (p_mmaps[i].valid == 0)
        continue;
    if ((p_mmaps[i].addr <= addr && addr <= p_mmaps[i].addr + p_mmaps[i].length) ||
      (p_mmaps[i].addr <= addr + length && addr + length <= p_mmaps[i].addr + p_mmaps[i].length)
    ) return FAILED;
  }

  // TO-DO: handle MAP_SHARED/ MAP_ANONYMOUS
  // TO-DO: lazy mapping
  for (i = 0; i < length / PGSIZE; i++) {
    char *mem = kalloc();
    if (mem == 0) return FAILED;
    pde_t *pgdir = myproc()->pgdir;
    mappages(pgdir, (void*)(uintptr_t)(addr + i*PGSIZE), PGSIZE, V2P((uintptr_t)mem), PTE_W | PTE_U);
  }

  // TO-DO: need locks?
  p_mmaps[free].addr = addr;
  p_mmaps[free].length = length;
  p_mmaps[free].flags = flags;
  p_mmaps[free].valid = 1;

  return addr;
}

int
sys_wunmap(void) {
  uint addr;

  if (argint(0, (int*)&addr) < 0)
    return FAILED;

  // addr should be valid and page divisible
  if (addr % PGSIZE != 0 || (addr < 0x60000000 || addr > KERNBASE))
    return FAILED;

  struct proc *curproc = myproc();
  struct mmap *p_mmaps = curproc->mmaps;

  int entry = -1;
  int length = 0;
  /*int flags = 0;*/
  for (int i = 0; i < MAX_WMMAP_INFO; i++) {
    if (p_mmaps[i].addr == addr) {
      entry = i;
      length = p_mmaps[i].length;
      /*flags = p_mmaps[i].flags;*/
      break;
    }
  }
  if (entry == -1)
      return FAILED;

  // TO-DO: lazy unmapping
  // TO-DO: handle MAP_SHARED/ MAP_ANONYMOUS
  pde_t *pgdir = myproc()->pgdir;
  for (int i = 0; i < length; i++) {
    pte_t *pte = walkpgdir(pgdir, (void*)(uintptr_t)addr + i*PGSIZE, 0);
    if (pte == 0) return FAILED;
    uint phys_addr = PTE_ADDR(*pte);
    kfree(P2V((uintptr_t)phys_addr));
    *pte = 0;
  }
  p_mmaps[entry].valid = 0;
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
  //struct proc *currproc = myproc();
  struct wmapinfo *info;

  if (argptr(0, (void *)&info, sizeof(struct wmapinfo)) < 0)
        return FAILED;

  return SUCCESS;
}
