// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

void initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void acquire(struct spinlock *lk)
{
  pushcli(); // disable interrupts to avoid deadlock.
  if (holding(lk))
    panic("acquire");

  // The xchg is atomic.
  while (xchg(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen after the lock is acquired.
  __sync_synchronize();

  // Record info about lock acquisition for debugging.
  lk->cpu = mycpu();
  // 호출 스택 정보를 기록
  getcallerpcs(&lk, lk->pcs);
}

// Release the lock.
void release(struct spinlock *lk)
{
  if (!holding(lk))
    panic("release");

  // 락 획득 정보 초기화
  lk->pcs[0] = 0;
  lk->cpu = 0;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other cores before the lock is released.
  // Both the C compiler and the hardware may re-order loads and
  // stores; __sync_synchronize() tells them both not to.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code can't use a C assignment, since it might
  // not be atomic. A real OS would use C atomics here.
  asm volatile("movl $0, %0" : "+m"(lk->locked) :);

  // 인터럽트 재활성화
  popcli();
}

// Record the current call stack in pcs[] by following the %ebp chain.
void getcallerpcs(void *v, uint pcs[])
{
  uint *ebp;
  int i;

  ebp = (uint *)v - 2;
  for (i = 0; i < 10; i++)
  {
    if (ebp == 0 || ebp < (uint *)KERNBASE || ebp == (uint *)0xffffffff)
      break;
    pcs[i] = ebp[1];      // saved %eip
    ebp = (uint *)ebp[0]; // saved %ebp
  }
  for (; i < 10; i++)
    pcs[i] = 0;
}

// Check whether this cpu is holding the lock.
int holding(struct spinlock *lock)
{
  int r;
  pushcli();
  r = lock->locked && lock->cpu == mycpu();
  popcli();
  return r;
}

// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.

// 인터럽트를 비활성화
void pushcli(void)
{
  int eflags;
  // readeflags() 함수를 호출하여 현재 CPU 상태 레지스터 값을 읽어옴
  eflags = readeflags();
  // cli() 어셈블리 명령어를 호출하여 인터럽트 비활성화
  cli();
  // 현재 CPU에서 인터럽트가 처음으로 비활성화되는 경우,
  // mycpu()->intena 변수에 이전 인터럽트 상태를 저장
  if (mycpu()->ncli == 0)
    mycpu()->intena = eflags & FL_IF;
  // 인터럽트 비활성화 횟수 증가
  mycpu()->ncli += 1;
}

// 인터럽트 활성화
void popcli(void)
{
  // 현재 인터럽트가 활성화되어 있는지 확인
  if (readeflags() & FL_IF)
    panic("popcli - interruptible");
  // 인터럽트 비활성화 횟수 감소
  if (--mycpu()->ncli < 0)
    // 인터럽트 비활성화 횟수가 음수가 되면 패닉 발생
    panic("popcli");
  // 인터럽트 비활성화 횟수가 0이 되면 인터럽트 활성화
  if (mycpu()->ncli == 0 && mycpu()->intena)
    sti();
}
