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
  int size;        // 할당/해제할 페이지 크기
  int delay_ticks; // 메모리 해제 지연 시간

  if (argint(0, &size) < 0 || argint(1, &delay_ticks) < 0)
    return -1;

  // 할당 크기가 0인 경우 -1 반환
  if (size == 0)
    return -1;

  // 페이지 크기의 배수가 아닌 경우 -1 반환
  if ((size % PGSIZE) != 0)
    return -1;

  struct proc *curproc = myproc(); // 현재 실행중인 프로세스

  // 메모리 할당 요청인 경우
  if (size > 0)
  {
    // 프로세스의 메모리 크기를 증가시키고 할당된 주소 반환
    uint addr = curproc->sz;
    curproc->sz += size;
    return addr;
  }

  // 메모리 해제 요청인 경우
  // 지연 시간이 0과 같거나 작은 경우, -1 반환
  if (delay_ticks <= 0)
    return -1;

  uint addr = curproc->sz + size; // 해제할 메모리 주소

  // 이미 진행 중인 메모리 해제 작업이 있는 경우, 해제할 메모리 크기 누적
  if (curproc->dealloc_size > 0)
  {
    curproc->dealloc_size += -size;
  }
  else
  {
    curproc->dealloc_size = -size;
  }

  curproc->dealloc_ticks = delay_ticks; // 해제 지연 시간 설정
  curproc->dealloc_start = ticks;       // 해제 요청 시작 시간 설정
  curproc->sz = addr;                   // 프로세스 메모리 크기 갱신

  // 메모리 해제 요청 시간 출력
  struct rtcdate r;
  cmostime(&r);
  cprintf("Memory deallocation request(%d): %d-%d-%d %d:%d:%d\n",
          delay_ticks, r.year, r.month, r.day, r.hour, r.minute, r.second);
  return addr;
}

int sys_memstat(void)
{
  struct proc *curproc = myproc();  // 현재 실행중인 프로세스
  pde_t *pgdir = curproc->pgdir;    // 현재 프로세스의 페이지 디렉토리
  uint sz = PGROUNDUP(curproc->sz); // 프로세스의 메모리 크기를 페이지 크기로 올림

  uint i;
  uint virt_pages = 0; // 가상 페이지 수
  uint phys_pages = 0; // 물리 페이지 수

  // 가상 메모리 크기를 페이지 크기로 나눠 가상 페이지 수를 계산
  virt_pages = sz / PGSIZE;

  // 페이지 테이블 주소 계산
  pte_t *pgtab = (pte_t *)P2V(PTE_ADDR(pgdir[0]));
  // 페이지 테이블을 순회하며 물리 페이지 수 계산
  for (i = 0; i < NPTENTRIES && i * PGSIZE < sz; i++)
  {
    if (pgtab[i] & PTE_P) // Present 비트가 설정된 엔트리만 고려
    {
      phys_pages++;
    }
  }

  // 가상 페이지 수와 물리 페이지 수 출력
  cprintf("vp: %d, pp: %d\n", virt_pages, phys_pages);
  // 페이지 디렉토리 엔트리 값 출력
  cprintf("PDE - 0x%x\n", pgdir[0]);

  cprintf("PTE");
  // 페이지 테이블을 순회하며 페이지 테이블 엔트리 값 출력
  for (i = 0; i < NPTENTRIES && i * PGSIZE < sz; i++)
  {
    if ((pgtab[i] & PTE_P) && (pgtab[i] & PTE_U)) // Present 비트와 User 비트가 설정된 엔트리만 고려
    {
      // 페이지 테이블 엔트리 값 출력
      cprintf(" - 0x%x", pgtab[i]);
    }
  }
  cprintf("\n");

  return 0;
}
