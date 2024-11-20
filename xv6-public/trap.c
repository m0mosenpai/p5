#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "stdint.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

extern unsigned char pagerefs[NPAGES];
extern pte_t* walkpgdir(pde_t *pgdir, const void *va, int alloc);
extern int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT:
    uint c_addr = PGROUNDDOWN(rcr2());
    struct proc *p = myproc();
    pde_t *pgdir = p->pgdir;
    pte_t *pte = walkpgdir(pgdir, (void*)c_addr, 0);
    uint pa = PTE_ADDR(*pte);
    int flags = PTE_FLAGS(*pte);
    struct mmap *p_mmaps = p->mmaps;

    /*cprintf("pageref 1: %d\n", pagerefs[pa >> PTXSHIFT]);*/
    if (pte && pagerefs[PFN(pa)] == 0) {
      pte = 0;
    }

    // check if pte exists
    if (pte) {
      /*if (*pte & PTE_P) {*/
      /*    cprintf("P set: %d\n", pagerefs[pa >> PTXSHIFT]);*/
      /*}*/
      /*cprintf("pageref 2: %d\n", pagerefs[pa >> PTXSHIFT]);*/
      if (*pte & PTE_OW) {
      /*cprintf("pageref 3: %d\n", pagerefs[pa >> PTXSHIFT]);*/
          if (pagerefs[PFN(pa)] == 1) {
            *pte |= PTE_W;
          }
          // copy on write
          else {
            /*cprintf("pageref 4: %d\n", pagerefs[pa >> PTXSHIFT]);*/
            char *mem = kalloc();
            if (mem == 0) p->killed = 1;
            else {
              /*cprintf("pageref 5: %d\n", pagerefs[pa >> PTXSHIFT]);*/
              memmove(mem, (char*)P2V(pa), PGSIZE);
              /*cprintf("pageref 6: %d\n", pagerefs[pa >> PTXSHIFT]);*/
              /*cprintf("addr: %p\n", c_addr);*/
              /**pte &= ~PTE_P;*/
              if (mappages(pgdir, (void*)c_addr, PGSIZE, V2P(mem), flags) < 0) {
                /*cprintf("pageref 7: %d\n", pagerefs[pa >> PTXSHIFT]);*/
                kfree(mem);
                p->killed = 1;
              }
              else {
                pagerefs[PFN(pa)]--;
                /*cprintf("pageref 8: %d\n", pagerefs[pa >> PTXSHIFT]);*/
              }
            }
          }
          lcr3(V2P(pgdir));
      }
      else {
        cprintf("Segmentation Fault\n");
        p->killed = 1;
      }
    }
    else {
      // lazy alloc
      int i;
      for (i = 0; i < MAX_WMMAP_INFO; i++) {
        int addr = p_mmaps[i].addr;
        int length = p_mmaps[i].length;
        struct file *file = p_mmaps[i].file;
        if (addr <= c_addr && c_addr < addr + length) {
          char *mem = kalloc();
          if (mem == 0) {
            p->killed = 1;
            break;
          }
          memset(mem, 0, PGSIZE);
          if (file != 0) {
            file->off = c_addr - addr;
            fileread(file, mem, PGSIZE);
          }
          if (mappages(pgdir, (void*)(c_addr), PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) {
            kfree(mem);
            p->killed = 1;
            break;
          }
          p_mmaps[i].nloaded++;
          break;
        }
      }
      if (i >= MAX_WMMAP_INFO) {
        cprintf("Segmentation Fault\n");
        exit();
        /*p->killed = 1;*/
      }
    }
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
