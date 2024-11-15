#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "wmap.h"
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
  if ((length <= 0)                                                ||
    (flags & MAP_FIXED) != MAP_FIXED                               ||
    (flags & MAP_SHARED) != MAP_SHARED                             ||
    (flags & MAP_ANONYMOUS) != MAP_ANONYMOUS && fd < 0             ||
    (addr % PGSIZE != 0 || (addr < 0x60000000 || addr > KERNBASE))
  ) return FAILED;

  // TO-DO: handle MAP_SHARED/ MAP_ANONYMOUS
  // TO-DO: lazy mapping
  int i;
  for (i = 0; i < length / PGSIZE; i++) {
    char *mem = kalloc();
    if (mem == 0) return FAILED;
    pde_t *pgdir = myproc()->pgdir;
    mappages(pgdir, (void*)(uintptr_t)(addr + i*PGSIZE), PGSIZE, V2P((uintptr_t)mem), PTE_W | PTE_U);
  }

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

  // TO-DO: loop and unmap all pages (when to end loop??)
  // TO-DO: lazy unmapping
  pde_t *pgdir = myproc()->pgdir;
  pte_t *pte = walkpgdir(pgdir, (void*)(uintptr_t)addr, 0);
  // addr should have a valid PTE
  if (pte == 0) return FAILED;

  uint phys_addr = PTE_ADDR(*pte);
  kfree(P2V((uintptr_t)phys_addr));
  *pte = 0;

  return SUCCESS;
}

int
sys_va2pa(void) {
  uint va;

  // TO-DO: translate va->pa and return pa
  if (argint(0, (int*)&va) < 0)
    return FAILED;

  return 0;
}

int
sys_getwmapinfo(void) {
  // TO-DO: populate struct
  return 0;
}
