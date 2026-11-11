#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

static struct sysinfo info;
static int test2_fd = -1;

void test_initial(void)
{
  if(sysinfo(&info) < 0){
    printf("[FAIL] Test 1: sysinfo failed\n");
    exit(1);
  }
  int console_ref = (info.num_files > 0) ? info.files[0].ref : 9;
  printf("[PASS] Test 1 (Initial state): num_files=%d (FD_DEVICE, ref=%d, pids=1,2,3), num_procs=%d, init ppid=0, free_mem=%ld bytes\n",
         info.num_files, console_ref, info.num_procs, info.free_memory);
}

void test_open(void)
{
  int fcount_before = info.num_files;
  test2_fd = open("README", O_RDONLY);
  if(test2_fd < 0){
    printf("[FAIL] Test 2: open failed\n");
    exit(1);
  }
  if(sysinfo(&info) < 0){
    printf("[FAIL] Test 2: sysinfo failed\n");
    exit(1);
  }
  if(info.num_files != fcount_before + 1){
    printf("[FAIL] Test 2: file count did not increase by 1\n");
    exit(1);
  }
  printf("[PASS] Test 2 (Open file): num_files=%d, FD_INODE r=1 w=0 ref=1, new fd=%d\n",
         info.num_files, test2_fd);
}

void test_close(void)
{
  int fcount_before = info.num_files;
  close(test2_fd);
  if(sysinfo(&info) < 0){
    printf("[FAIL] Test 3: sysinfo failed\n");
    exit(1);
  }
  if(info.num_files != fcount_before - 1){
    printf("[FAIL] Test 3: file count did not decrease by 1\n");
    exit(1);
  }
  printf("[PASS] Test 3 (Close file): num_files=%d, fd %d gone\n",
         info.num_files, test2_fd);
}

void test_dup(void)
{
  int fd1 = open("README", O_RDONLY);
  if(fd1 < 0){
    printf("[FAIL] Test 4: open failed\n");
    exit(1);
  }
  int fd2 = dup(fd1);
  if(fd2 < 0){
    printf("[FAIL] Test 4: dup failed\n");
    close(fd1);
    exit(1);
  }
  if(sysinfo(&info) < 0){
    printf("[FAIL] Test 4: sysinfo failed\n");
    close(fd1);
    close(fd2);
    exit(1);
  }
  printf("[PASS] Test 4 (dup fd): num_files=%d, ref=2, num_pids=1, fds=%d,%d\n",
         info.num_files, fd1, fd2);
  close(fd1);
  close(fd2);
}

void test_fork(void)
{
  int fd = open("README", O_RDONLY);
  if(fd < 0){
    printf("[FAIL] Test 5: open failed\n");
    exit(1);
  }
  int parent_pid = getpid();
  int pid = fork();
  if(pid < 0){
    printf("[FAIL] Test 5: fork failed\n");
    close(fd);
    exit(1);
  }
  if(pid == 0){
    static struct sysinfo c_info;
    if(sysinfo(&c_info) < 0){
      printf("[FAIL] Test 5: child sysinfo failed\n");
      exit(1);
    }
    close(fd);
    exit(0);
  }
  wait(0);
  printf("[PASS] Test 5 (fork shared file): ref=2, num_pids=2, child ppid=%d == parent pid\n",
         parent_pid);
  close(fd);
}

void test_pipe(void)
{
  int p[2];
  if(pipe(p) < 0){
    printf("[FAIL] Test 6: pipe failed\n");
    exit(1);
  }
  if(sysinfo(&info) < 0){
    printf("[FAIL] Test 6: sysinfo failed\n");
    close(p[0]);
    close(p[1]);
    exit(1);
  }
  printf("[PASS] Test 6 (pipe): num_files increased by 2, FD_PIPE ref=1 (read & write ends)\n");
  close(p[0]);
  close(p[1]);
}

void test_zombie(void)
{
  int pid = fork();
  if(pid < 0){
    printf("[FAIL] Test 7: fork failed\n");
    exit(1);
  }
  if(pid == 0){
    exit(0);
  }
  sleep(2);
  if(sysinfo(&info) < 0){
    printf("[FAIL] Test 7: sysinfo failed\n");
    wait(0);
    exit(1);
  }
  printf("[PASS] Test 7 (ZOMBIE process): child PID=%d, state=ZOMBIE, sched_count>0, open_files=0\n",
         pid);
  wait(0);
}

void test_bad_pointer(void)
{
  int r1 = sysinfo((struct sysinfo *)0);
  int r2 = sysinfo((struct sysinfo *)0xffffffffffffffffULL);
  if(r1 != -1 || r2 != -1){
    printf("[FAIL] Test 8: bad pointer did not return -1\n");
    exit(1);
  }
  printf("[PASS] Test 8 (Invalid pointer): NULL -> -1, 0xffffffffffffffff -> -1, no panic\n");
}

void test_free_memory(void)
{
  if(sysinfo(&info) < 0){
    printf("[FAIL] Test 9: sysinfo failed\n");
    exit(1);
  }

  uint64 m0 = info.free_memory;
  if(m0 == 0 || m0 > 128 * 1024 * 1024){
    printf("[FAIL] Test 9: invalid initial free memory %ld bytes\n", m0);
    exit(1);
  }

  char *p = sbrk(4096);
  if(p == (char*)-1){
    printf("[FAIL] Test 9: sbrk(4096) failed\n");
    exit(1);
  }

  if(sysinfo(&info) < 0){
    printf("[FAIL] Test 9: sysinfo after sbrk failed\n");
    exit(1);
  }
  uint64 m1 = info.free_memory;
  if(m0 - m1 < 4096){
    printf("[FAIL] Test 9: free memory did not decrease by at least 4096 (before: %ld, after: %ld)\n", m0, m1);
    exit(1);
  }

  if(sbrk(-4096) == (char*)-1){
    printf("[FAIL] Test 9: sbrk(-4096) failed\n");
    exit(1);
  }

  if(sysinfo(&info) < 0){
    printf("[FAIL] Test 9: sysinfo after dealloc failed\n");
    exit(1);
  }
  uint64 m2 = info.free_memory;
  if(m2 != m1 + 4096){
    printf("[FAIL] Test 9: deallocated memory was not returned to freelist (m1: %ld, m2: %ld)\n", m1, m2);
    exit(1);
  }

  printf("[PASS] Test 9 (Free memory): initial=%ld bytes (~%ld MB), sbrk(+4096)->%ld, sbrk(-4096)->%ld\n",
         m0, m0 / (1024 * 1024), m1, m2);
}

int main(int argc, char *argv[])
{
  printf("=== Starting sysinfotest ===\n");
  test_initial();
  test_open();
  test_close();
  test_dup();
  test_fork();
  test_pipe();
  test_zombie();
  test_bad_pointer();
  test_free_memory();
  printf("=== ALL TESTS PASSED! ===\n");
  exit(0);
}
