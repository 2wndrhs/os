#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
// 현재 실행 중인 CPU의 ID 반환
int cpuid()
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
// 현재 실행 중인 CPU의 구조체 포인터 반환
struct cpu *
mycpu(void)
{
  int apicid, i;

  // 인터럽트가 활성화되어 있는지 확인
  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  // lapicid() 함수를 호출하여 현재 CPU의 APIC ID를 읽어옴
  apicid = lapicid();

  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].

  // `cpus` 배열을 순회하며 APIC ID가 일치하는 CPU 구조체를 찾아 해당 구조체의 포인터를 반환
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }

  // 일치하는 APIC ID를 찾지 못하면 패닉 발생
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
// 현재 CPU에서 실행 중인 프로세스의 구조체 포인터 반환
struct proc *
myproc(void)
{
  struct cpu *c;
  struct proc *p;
  // 인터럽트 비활성화 (함수 실행 중에 프로세스가 다시 스케줄링 되는 것을 방지)
  pushcli();
  // 현재 실행 중인 CPU 구조체 포인터 얻기
  c = mycpu();
  // 현재 CPU에서 실행 중인 프로세스의 구조체 포인터 얻기
  p = c->proc;
  // 인터럽트 재활성화
  popcli();
  return p;
}

// PAGEBREAK: 32
//  Look in the process table for an UNUSED proc.
//  If found, change state to EMBRYO and initialize
//  state required to run in the kernel.
//  Otherwise return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// PAGEBREAK: 32
//  Set up first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); // DOC: wait-sleep
  }
}

// PAGEBREAK: 42
//  Per-CPU process scheduler.
//  Each CPU calls scheduler() after setting itself up.
//  Scheduler never returns.  It loops, doing:
//   - choose a process to run
//   - swtch to start running that process
//   - eventually that process transfers control
//       via swtch back to the scheduler.

// 무한 루프를 돌면서 실행 가능한 프로세스를 찾아 실행
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    // acquire() 함수를 호출하여 프로세스 테이블 락 획득
    acquire(&ptable.lock);
    // ptable.proc 배열을 순회하면서 프로세스의 상태가 RUNNABLE인 프로세스를 찾음
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;        // 현재 CPU에서 실행할 프로세스를 설정
      switchuvm(p);       // 사용자 가상 메모리 공간으로 전환
      p->state = RUNNING; // 프로세스의 상태를 RUNNING으로 변경

      // swtch() 함수를 호출하여 현재 CPU 컨텍스트를 저장하고,
      // 선택된 프로세스의 컨텍스트로 전환
      swtch(&(c->scheduler), p->context);

      // 프로세스가 실행을 마치고 다시 스케줄러로 돌아오면
      // 커널 가상 메모리 공간으로 전환
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }

    // 프로세스 테이블 락 해제
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
// 현재 실행 중인 프로세스를 스케줄러로 넘기고, 다른 프로세스를 실행할 수 있도록 함
void sched(void)
{
  int intena;
  // myproc() 함수를 호출하여 현재 실행 중인 프로세스의 구조체 포인터 획득
  struct proc *p = myproc();

  // 프로세스 테이블 락이 획득되었는지 확인
  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  // 인터럽트 비활성화 횟수가 1인지 확인
  if (mycpu()->ncli != 1)
    panic("sched locks");
  // 현재 프로세스의 상태가 RUNNING인지 확인
  // 프로세스가 실행 중인 상태에서 스케줄러로 전환되는 것을 방지
  if (p->state == RUNNING)
    panic("sched running");
  // 인터럽트가 활성화되어 있는지 확인
  // 스케줄러로 전환되는 동안 인터럽트가 발생하지 않도록 함
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  // 현재 CPU의 인터럽트 상태를 저장
  // 스케줄러로 전환한 후에 인터럽트 상태를 복원하기 위함
  intena = mycpu()->intena;
  // 현재 프로세스의 컨텍스트를 저장하고 스케줄러의 컨텍스트로 전환
  swtch(&p->context, mycpu()->scheduler);
  // 스케줄러가 다시 원래의 프로세스를 실행하기로 결정하면
  // 이전에 저장된 프로세스의 컨텍스트로 복원됨
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
// 현재 실행 중인 프로세스가 CPU를 포기하고
// 다른 프로세스가 실행될 수 있도록 함
void yield(void)
{
  // 프로세스 테이블 락 획득 (데이터 일관성 유지)
  acquire(&ptable.lock); // DOC: yieldlock
  // myproc() 함수를 호출하여 현재 실행중인 프로세스의 구조체 포인터 획득
  // 현재 프로세스의 상태를 RUNNABLE로 변경 (프로세스가 실행 가능한 상태암을 나타냄)
  myproc()->state = RUNNABLE;
  // sched() 함수를 호출하여 스케줄러로 제어를 넘김
  sched();
  // 프로세스 테이블 락 해제
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// 현재 실행 중인 프로세스를 지정된 채널에서 잠들게 하고,
// 다른 프로세스가 실행될 수 있도록 함
void sleep(void *chan, struct spinlock *lk)
{
  // 현재 실행 중인 프로세스의 구조체 포인터 획득
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.

  // 제공된 락이 ptable.lock이 아닌 경우,
  // ptable.lock을 획득하고 제공된 락을 해제
  if (lk != &ptable.lock)
  {                        // DOC: sleeplock0
    acquire(&ptable.lock); // DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;      // 프로세스의 chan 필드를 지정된 채널로 설정
  p->state = SLEEPING; // 프로세스의 상태를 SLEEPING으로 변경

  // sched() 함수를 호출하여 현재 프로세스를 스케줄러로 넘기고,
  // 다른 프로세스를 실행할 수 있도록 함
  // sched() 함수는 내부적으로 swtch() 함수를 호출하여 스케줄러 컨텍스트로 전환
  sched();

  // 프로세스가 깨어나면, `chan` 필드를 0으로 설정하여 초기화
  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  // ptable.lock이 아닌 락을 제공한 경우,
  // ptable.lock을 해제하고 제공된 락을 다시 획득
  if (lk != &ptable.lock)
  { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// PAGEBREAK!
//  Wake up all processes sleeping on chan.
//  The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
  // 프로세스 테이블을 순회하면서 지정된 채널에서 잠들고 있는 모든 프로세스를 찾음
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE; // 프로세스의 상태를 RUNNABLE로 변경
}

// Wake up all processes sleeping on chan.
// 지정된 채널에서 잠들고 있는 모든 프로세스를 깨움
void wakeup(void *chan)
{
  // 프로세스 테이블 락 획득
  acquire(&ptable.lock);
  // wakeup1() 함수를 호출하여 지정된 채널에서 잠들고 있는 모든 프로세스를 깨움
  wakeup1(chan);
  // 프로세스 테이블 락 해제
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// PAGEBREAK: 36
//  Print a process listing to console.  For debugging.
//  Runs when user types ^P on console.
//  No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
