#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[]; // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

// External declaration of the process table
extern struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

void tvinit(void)
{
  int i;

  for (i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE << 3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE << 3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void idtinit(void)
{
  lidt(idt, sizeof(idt));
}

// 각 큐 레벨의 타임 슬라이스
static int time_slice[4] = {10, 20, 40, 80};

// 프로세스의 상태에 따라 프로세스의 상태를 업데이트
void update_process_state(struct proc *p)
{
  switch (p->state)
  {
  // 프로세스의 상태가 RUNNING인 경우
  case RUNNING:
    p->cpu_burst++;              // 타임 슬라이스 내 CPU 사용 시간 증가
    p->total_cpu_time++;         // 총 CPU 사용 시간 증가
    p->time_slice_exhausted = 0; // 타임 슬라이스를 모두 사용하지 않은 상태로 변경

    // 프로세스의 총 CPU 사용 시간이 프로세스가 필요한 CPU 할당량을 넘은 경우
    if (p->end_time > 0 && p->total_cpu_time >= p->end_time)
    {
// 디버깅 메시지 출력
#ifdef DEBUG
      cprintf("PID: %d uses %d ticks in mlfq[%d], total(%d/%d)\n", p->pid, p->cpu_burst, p->q_level, p->total_cpu_time, p->end_time);
      cprintf("PID: %d used %d ticks. terminated\n", p->pid, p->total_cpu_time);
#endif

      p->killed = 1; // 프로세스의 killed 변수를 1로 설정하여 프로세스 종료
      return;
    }

    // 타임 슬라이스 내 CPU 사용 시간이 해당 큐 레벨의 타임 슬라이스를 넘은 경우
    if (p->cpu_burst >= time_slice[p->q_level])
    {
      // 타임 슬라이스를 모두 사용한 상태로 변경
      p->time_slice_exhausted = 1;
// 디버깅 출력
#ifdef DEBUG
      cprintf("PID: %d uses %d ticks in mlfq[%d], total(%d/%d)\n", p->pid, p->cpu_burst, p->q_level, p->total_cpu_time, p->end_time);
#endif
      // 프로세스가 위치한 큐 레벨이 3보다 작은 경우
      if (p->q_level < 3)
      {
        // 더 낮은 우선순위 큐로 이동
        p->q_level++;
        // 큐 진입 시간 업데이트
        p->queue_enter_time = ticks;
        // 타임 슬라이스 내 CPU 사용 시간, CPU 대기 시간, I/O 대기 시간 초기화
        p->cpu_burst = 0;
        p->cpu_wait = 0;
        p->io_wait_time = 0;
      }
      // 프로세스가 위치한 큐 레벨이 3인 경우 (최하위 큐)
      else
      {
        // 타임 슬라이스 내 CPU 사용 시간만 초기화
        p->cpu_burst = 0;
      }
    }
    break;
  // 프로세스의 상태가 RUNNABLE인 경우
  case RUNNABLE:
    // CPU 대기 시간 증가
    p->cpu_wait++;

    // Aging 매커니즘 (CPU 대기 시간이 250 이상인 경우)
    if (p->cpu_wait >= 250 && p->q_level > 0)
    {
// 디버깅 출력
#ifdef DEBUG
      cprintf("PID: %d Aging\n", p->pid);
#endif
      // 더 높은 우선순위 큐로 이동
      p->q_level--;
      // 큐 진입 시간 업데이트
      p->queue_enter_time = ticks;
      // 타임 슬라이스 내 CPU 사용 시간, CPU 대기 시간, I/O 대기 시간 초기화
      p->cpu_burst = 0;
      p->cpu_wait = 0;
      p->io_wait_time = 0;
    }
    break;
  // 프로세스의 상태가 SLEEPING인 경우
  case SLEEPING:
    // I/O 대기 시간 증가
    p->io_wait_time++;
    break;
  }
}

// PAGEBREAK: 41
void trap(struct trapframe *tf)
{
  if (tf->trapno == T_SYSCALL)
  {
    if (myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }

  switch (tf->trapno)
  {
  case T_IRQ0 + IRQ_TIMER:
    if (cpuid() == 0)
    {
      acquire(&tickslock);
      ticks++;

      // 현재 실행중인 프로세스의 구조체 포인터 획득
      struct proc *p = myproc();
      // idle, init, shell 프로세스 스킵
      if (p != 0 && p->pid > 2)
      {
        // 프로세스 테이블 락 획득
        acquire(&ptable.lock);
        // 매 타이머 틱마다 프로세스의 상태를 업데이트
        update_process_state(p);
        // 현재 실행중인 프로세스 이외의 모든 프로세스의 상태를 업데이트
        struct proc *other;
        for (other = ptable.proc; other < &ptable.proc[NPROC]; other++)
        {
          if (other != p)
          {
            update_process_state(other);
          }
        }
        // 프로세스 테이블 락 해제
        release(&ptable.lock);
      }

      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE + 1:
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

  // PAGEBREAK: 13
  default:
    if (myproc() == 0 || (tf->cs & 3) == 0)
    {
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
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if (myproc() && myproc()->state == RUNNING &&
      tf->trapno == T_IRQ0 + IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();
}
