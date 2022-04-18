#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
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

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  // exit(1);
  backtrace();
  // printf("zju comming!\n");
  // panic("no print\n");
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

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


int
sys_sigreturn(void)
{
  struct proc* p = myproc();

  p->trapframe->a0 = p->copyframe->a0;
  p->trapframe->a1 = p->copyframe->a1;
  p->trapframe->a2 = p->copyframe->a2;
  p->trapframe->a3 = p->copyframe->a3;
  p->trapframe->a4 = p->copyframe->a4;
  p->trapframe->a5 = p->copyframe->a5;
  p->trapframe->a6 = p->copyframe->a6;
  p->trapframe->a7 = p->copyframe->a7;

  p->trapframe->s0 = p->copyframe->s0;
  p->trapframe->s1 = p->copyframe->s1;
  p->trapframe->s2 = p->copyframe->s2;
  p->trapframe->s3 = p->copyframe->s3;
  p->trapframe->s4 = p->copyframe->s4;
  p->trapframe->s5 = p->copyframe->s5;
  p->trapframe->s6 = p->copyframe->s6;
  p->trapframe->s7 = p->copyframe->s7;
  p->trapframe->s8 = p->copyframe->s8;
  p->trapframe->s9 = p->copyframe->s9;
  p->trapframe->s10 = p->copyframe->s10;
  p->trapframe->s11 = p->copyframe->s11;

  p->trapframe->t0 = p->copyframe->t0;
  p->trapframe->t1 = p->copyframe->t1;
  p->trapframe->t2 = p->copyframe->t2;
  p->trapframe->t3 = p->copyframe->t3;
  p->trapframe->t4 = p->copyframe->t4;
  p->trapframe->t5 = p->copyframe->t5;
  p->trapframe->t6 = p->copyframe->t6;

  p->trapframe->tp = p->copyframe->tp;
  p->trapframe->gp = p->copyframe->gp;
  p->trapframe->sp = p->copyframe->sp;
  p->trapframe->ra = p->copyframe->ra;
  p->trapframe->epc = p->copyframe->epc;

  p->busyflag = 0;
  // printf("comming zju\n");
  return 0;
}

int 
sys_sigalarm(void)
{
  struct proc* p = myproc();
  p->interval = (int)(p->trapframe->a0);
  p->handlfunct = (void(*)())(p->trapframe->a1);
  // printf("whereisfunct: %p\n", p->handlfunct);
  p->leftticks = p->interval;
  return 0;
}