#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
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
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_ssusbrk(void)
{
  int size;
  int delay_ticks;
  if (argint(0, &size) < 0 || argint(1, &delay_ticks) < 0)
    return -1;

  if (size == 0)
    return -1;

  // Check if size is multiple of PGSIZE
  if ((size % PGSIZE) != 0)
    return -1;

  struct proc *curproc = myproc();

  if (size > 0)
  {
    uint addr = curproc->sz;
    curproc->sz += size;
    return addr;
  }

  // Memory deallocation
  if (delay_ticks <= 0)
    return -1;

  uint addr = curproc->sz + size;

  // If there's already a pending deallocation, add to it
  if (curproc->dealloc_size > 0)
  {
    curproc->dealloc_size += -size;
  }
  else
  {
    curproc->dealloc_size = -size;
  }

  // Update ticks with the new delay
  curproc->dealloc_ticks = delay_ticks;
  curproc->dealloc_start = ticks;
  curproc->sz = addr;

  cprintf("Deallocation requested at tick: %d\n", ticks);
  cprintf("Total pending deallocation size: %d\n", curproc->dealloc_size);
  return addr;
}

int sys_memstat(void)
{
  struct proc *curproc = myproc();
  pde_t *pgdir = curproc->pgdir;
  uint i, j;
  uint virt_pages = 0;
  uint phys_pages = 0;

  // Count pages
  for (i = 0; i < NPDENTRIES; i++)
  {
    if (pgdir[i] & PTE_P)
    {
      pte_t *pgtab = (pte_t *)P2V(PTE_ADDR(pgdir[i]));
      cprintf("PDE %d: %x\n", i, pgdir[i]);

      for (j = 0; j < NPTENTRIES; j++)
      {
        if (pgtab[j] & PTE_P)
        {
          phys_pages++;
          cprintf("  PTE %d: %x\n", j, pgtab[j]);
        }
        if (pgtab[j] & (PTE_P | PTE_U))
          virt_pages++;
      }
    }
  }

  cprintf("Virtual pages: %d\n", virt_pages);
  cprintf("Physical pages: %d\n", phys_pages);

  return 0;
}
