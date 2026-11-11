#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

extern struct proc proc[NPROC];
extern struct spinlock wait_lock;
extern struct spinlock sysinfo_lock;

static struct sysinfo k_sysinfo;
static struct file *ofile_snap[NPROC][NOFILE];
static int pid_snap[NPROC];

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
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

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
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

  argint(0, &pid);
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


uint64
sys_trace(void)
{
  int mask;
  // Lấy tham số nguyên thứ 0 truyền vào từ user space
  argint(0, &mask);
  myproc()->trace_mask = mask;
  return 0;
}


uint64
sys_sysinfo(void)
{
  uint64 addr;
  argaddr(0, &addr);

  acquire(&sysinfo_lock);
  memset(&k_sysinfo, 0, sizeof(k_sysinfo));
  memset(ofile_snap, 0, sizeof(ofile_snap));
  memset(pid_snap, 0, sizeof(pid_snap));

  // Thu thập số byte bộ nhớ vật lý còn rảnh
  k_sysinfo.free_memory = count_free_bytes();

  // Pha 1: Quét proc[] thu thập tiến trình và chụp ofile_snap, pid_snap
  int nprocs = 0;
  for(int i = 0; i < NPROC; i++){
    struct proc *p = &proc[i];
    acquire(&wait_lock);
    acquire(&p->lock);
    if(p->state != UNUSED){
      k_sysinfo.procs[nprocs].pid = p->pid;
      k_sysinfo.procs[nprocs].state = (int)p->state;
      k_sysinfo.procs[nprocs].sched_count = p->sched_count;
      k_sysinfo.procs[nprocs].ppid = (p->parent) ? p->parent->pid : 0;

      pid_snap[nprocs] = p->pid;
      int num_fds = 0;
      for(int fd = 0; fd < NOFILE; fd++){
        ofile_snap[nprocs][fd] = p->ofile[fd];
        if(p->ofile[fd] != 0){
          k_sysinfo.procs[nprocs].open_files[num_fds++] = fd;
        }
      }
      k_sysinfo.procs[nprocs].num_open_files = num_fds;
      nprocs++;
    }
    release(&p->lock);
    release(&wait_lock);
  }
  k_sysinfo.num_procs = nprocs;

  // Pha 2: Gọi get_file_info() quét ftable.file[] và đối chiếu với ofile_snap
  get_file_info(&k_sysinfo, ofile_snap, pid_snap, nprocs);

  // Sao chép k_sysinfo về không gian user bằng copyout()
  if(addr == 0 || copyout(myproc()->pagetable, addr, (char *)&k_sysinfo, sizeof(k_sysinfo)) < 0){
    release(&sysinfo_lock);
    return -1;
  }

  release(&sysinfo_lock);
  return 0;
}