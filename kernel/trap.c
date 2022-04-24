#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else if(r_scause() == 15)
  {
    if(cowpagefault(p->pagetable, r_stval()) < 0)
      p->killed = 1;
  }
  else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

int
cowpagefault(pagetable_t pagetable, uint64 va)
{

  // printf("mou~~  pid: %d  va:%p\n", myproc()->pid, va);
  uint64 oldpa, newpa;
  pte_t *oldpte, *newpte;
  if(va > MAXVA || va > myproc()->sz)
    return -1;
  if((va < myproc()->trapframe->sp) && va > PGROUNDUP(myproc()->trapframe->gp))
    return -1;
  oldpte = walk(pagetable, va, 0);
  if(oldpte == 0)
    return -1;
  if((*oldpte & PTE_U) == 0)
    return -1;
  if((*oldpte & PTE_V) == 0)
    return -1;
  if((*oldpte & PTE_C) == 0 || (*oldpte & PTE_W) != 0)
    return -1;
  
  oldpa = PTE2PA(*oldpte);
  if(cow_linkacquire(oldpa) == 1){
    newpa = oldpa;
    newpte = oldpte;
    *newpte = (*newpte | PTE_W) & ~PTE_C;
    return 0;
  }
  kfree((char*)oldpa);
  if((newpa = (uint64)kalloc()) == 0)
    return -1;
  uint64 flags = (PTE_FLAGS(*oldpte) | PTE_W) & ~PTE_C;
  memmove((char*)newpa, (char*)oldpa, PGSIZE);
  uvmunmap(pagetable, PGROUNDDOWN(va), 1, 0);
  mappages(pagetable, PGROUNDDOWN(va), PGSIZE, newpa, flags);
  return 0;


  // pte_t *pte;
  // uint64 oldpa, newpa;
  // // va >= MAXVA cause walk() panic
  // if (va >= MAXVA)
  //   return -1;
  // if ((pte = walk(pagetable, va, 0)) == 0)
  //   return -1;
  // if ((*pte & PTE_C) == 0)
  //   return -1;
  // if (*pte & PTE_W)
  //   return -1;
  // // guard pae, trampoline, trapframe
  // if ((*pte & PTE_U) == 0) {
  //   printf("PTE_U=0\n");
  //   return -1;
  // }
  // oldpa = PTE2PA(*pte);
  // // Important optimization: if a store page fault and
  // // the ref which pointed this phy-page is one, no copy.
  // if (cow_linkacquire(oldpa) == 1) {
  //   *pte = (*pte | PTE_W) & (~PTE_C);
  //   return 0;
  // }
  // if ((newpa = (uint64)kalloc()) == 0) {
  //   printf("cowpagefault: kalloc fault.\n");
  //   return -1;
  // }
  // // copy the old page to the new page
  // memmove((void*)newpa, (void*)oldpa, PGSIZE);
  // *pte = (PA2PTE(newpa) | PTE_FLAGS(*pte) | PTE_W) & (~PTE_C);
  // kfree((void*)oldpa);
  // return 0;
}
//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p  pid:%d\n", scause, myproc()->pid);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

