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

// 현재 프로세스를 지정된 시간 동안 잠들게 하여,
// 다른 프로세스가 실행될 수 있도록 함
int sys_sleep(void)
{
  int n;
  uint ticks0;

  // 시스템 콜의 첫 번째 인자로부터 잠들 시간 가져오기
  if (argint(0, &n) < 0)
    return -1;
  // tickslock을 획득하여 clock tick을 안전하게 읽음
  acquire(&tickslock);
  // 현재 clock tick을 가져와서 ticks0에 저장
  ticks0 = ticks;
  // 지정된 시간이 경과할 때까지 현재 프로세스를 잠들게 함
  while (ticks - ticks0 < n)
  {
    // 현재 프로세스가 종료되었는지 확인
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    // sleep() 함수를 호출하여 현재 프로세스를 잠들게 함
    sleep(&ticks, &tickslock);
  }
  // tickslock을 해제
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

int sys_set_proc_info(void)
{
  int q_level, cpu_burst, cpu_wait_time, io_wait_time, end_time;

  if (argint(0, &q_level) < 0 || argint(1, &cpu_burst) < 0 ||
      argint(2, &cpu_wait_time) < 0 || argint(3, &io_wait_time) < 0 ||
      argint(4, &end_time) < 0)
    return -1;

  return set_proc_info(q_level, cpu_burst, cpu_wait_time, io_wait_time, end_time);
}