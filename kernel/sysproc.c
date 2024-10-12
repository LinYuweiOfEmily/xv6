#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0) return -1;
  exit(n);
  return 0;  // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  uint64 p1;
  if (argaddr(0, &p) < 0) return -1;
  if (argaddr(1, &p1) < 0) return -1;
  return wait(p, p1);
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0) return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if (argint(0, &n) < 0) return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0) return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_rename(void) {
  char name[16];
  int len = argstr(0, name, MAXPATH);
  if (len < 0) {
    return -1;
  }
  struct proc *p = myproc();
  memmove(p->name, name, len);
  p->name[len] = '\0';
  return 0;
}

uint64 sys_yield(void) {

//   获取当前正在执行的进程PCB(参考mycpu和myproc)
  struct proc *myP = myproc();


// 打印出该进程对应的内核线程在进行上下文切换时，上下文被保存到的地址区间(参考上下文切换)；
  

  printf("Save the context of the process to the memory region from address %p to %p\n", &myP->context, (&myP->context) + 1);

// 打印出该进程的用户态陷入内核态时PC的值(参考trapframe)；
  printf("Current running process pid is %d and user pc is %p\n", myP->pid, myP->trapframe->epc);  

// 根据调度器的工作方式模拟一次调度，找到下一个RUNNABLE的进程，同样打印相关信息(参考调度器线程的工作方式)。首先需要在proc.h函数末尾新增extern声明全局进程表，然后在sys_yield函数中从当前进程起，环形遍历全局进程表，在这个过程中记得注意锁的获取和释放。

  struct proc *p;




 

  for (p = myP + 1; ;p++) {
    
    if (p >= &proc[NPROC]) {
      p = proc;
    }
    if (p == myP) {
      break;
    }
    
    acquire(&p->lock);
    if (p->state == RUNNABLE ) {

      printf("Next runnable process pid is %d and user pc is %p\n", p->pid, p->trapframe->epc);

      release(&p->lock);
      break;
    }

    release(&p->lock);
    
  }

// 然后将当前进程挂起，XV6内核态已经帮我们实现了一个yield函数了。

  yield();

  return 0;
}
