#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/sysinfo.h"

static char *state_names[] = {
  [S_UNUSED]   "UNUSED  ",
  [S_USED]     "USED    ",
  [S_SLEEPING] "SLEEPING",
  [S_RUNNABLE] "RUNNABLE",
  [S_RUNNING]  "RUNNING ",
  [S_ZOMBIE]   "ZOMBIE  "
};

static char *type_names[] = {
  [S_FD_NONE]   "NONE  ",
  [S_FD_PIPE]   "PIPE  ",
  [S_FD_INODE]  "INODE ",
  [S_FD_DEVICE] "DEVICE"
};

void
print_sysinfo(struct sysinfo *info)
{
  printf("\n--- SYSTEM INFO REPORT ---\n");
  printf("Total Active Processes: %d\n", info->num_procs);
  printf("PID\tPPID\tSTATE   \tSCHED_EVENTS\tOPEN_FDS\n");
  for(int i = 0; i < info->num_procs; i++){
    struct proc_info *p = &info->procs[i];
    char *st = (p->state >= 0 && p->state <= S_ZOMBIE) ? state_names[p->state] : "UNKNOWN ";
    printf("%d\t%d\t%s\t%d          \t[", p->pid, p->ppid, st, p->sched_count);
    for(int j = 0; j < p->num_open_files; j++){
      printf("%d%s", p->open_files[j], (j + 1 < p->num_open_files) ? ", " : "");
    }
    printf("]\n");
  }

  printf("\nTotal Open Files: %d\n", info->num_files);
  printf("IDX\tTYPE  \tMODE\tREF\tREFERENCING PIDS\n");
  for(int i = 0; i < info->num_files; i++){
    struct file_info *f = &info->files[i];
    char *tp = (f->type >= 0 && f->type <= S_FD_DEVICE) ? type_names[f->type] : "UNKNOWN";
    char mode[4] = "---";
    if(f->readable && f->writable)
      strcpy(mode, "RW ");
    else if(f->readable)
      strcpy(mode, "R  ");
    else if(f->writable)
      strcpy(mode, "W  ");

    printf("%d\t%s\t%s\t%d\t[", i, tp, mode, f->ref);
    for(int j = 0; j < f->num_pids; j++){
      printf("%d%s", f->pids[j], (j + 1 < f->num_pids) ? ", " : "");
    }
    printf("]\n");
  }
  printf("--------------------------\n\n");
}

static struct sysinfo info;

void
test_initial(void)
{
  printf("[TEST 1] Initial system state at startup...\n");
  if(sysinfo(&info) < 0){
    printf("[TEST 1 FAIL] sysinfo call failed\n");
    exit(1);
  }
  print_sysinfo(&info);
  if(info.num_procs < 2 || info.num_files < 1){
    printf("[TEST 1 FAIL] Unexpected initial procs or files count\n");
    exit(1);
  }
  printf("[TEST 1 PASS] Initial state verified.\n\n");
}

void
test_open_close(void)
{
  printf("[TEST 2] Testing open() and close() file...\n");
  sysinfo(&info);
  int fcount_before = info.num_files;
  int my_pid = getpid();

  int fd = open("test_sysinfo.tmp", O_CREATE | O_RDWR);
  if(fd < 0){
    printf("[TEST 2 FAIL] open failed\n");
    exit(1);
  }

  sysinfo(&info);
  if(info.num_files != fcount_before + 1){
    printf("[TEST 2 FAIL] File count did not increase by 1 (before: %d, after: %d)\n", fcount_before, info.num_files);
    close(fd);
    unlink("test_sysinfo.tmp");
    exit(1);
  }

  // Find the opened file and check if my_pid is in it
  int found_pid = 0;
  for(int i = 0; i < info.num_files; i++){
    for(int j = 0; j < info.files[i].num_pids; j++){
      if(info.files[i].pids[j] == my_pid){
        found_pid = 1;
        break;
      }
    }
  }
  if(!found_pid){
    printf("[TEST 2 FAIL] Current process PID not found in open file references\n");
    close(fd);
    unlink("test_sysinfo.tmp");
    exit(1);
  }

  close(fd);
  unlink("test_sysinfo.tmp");

  sysinfo(&info);
  if(info.num_files != fcount_before){
    printf("[TEST 2 FAIL] File count did not decrease back after close\n");
    exit(1);
  }
  printf("[TEST 2 PASS] Open and close verified.\n\n");
}

void
test_dup(void)
{
  printf("[TEST 3] Testing dup() file descriptor...\n");
  sysinfo(&info);
  int fcount_before = info.num_files;

  int fd1 = open("test_dup.tmp", O_CREATE | O_RDWR);
  if(fd1 < 0){
    printf("[TEST 3 FAIL] open failed\n");
    exit(1);
  }
  int fd2 = dup(fd1);
  if(fd2 < 0){
    printf("[TEST 3 FAIL] dup failed\n");
    close(fd1);
    unlink("test_dup.tmp");
    exit(1);
  }

  sysinfo(&info);
  // Total distinct files in ftable should only increase by 1
  if(info.num_files != fcount_before + 1){
    printf("[TEST 3 FAIL] File count mismatch on dup (expected: %d, got: %d)\n", fcount_before + 1, info.num_files);
    close(fd1);
    close(fd2);
    unlink("test_dup.tmp");
    exit(1);
  }

  // Verify ref count is 2 for this file
  int found_ref2 = 0;
  for(int i = 0; i < info.num_files; i++){
    if(info.files[i].type == S_FD_INODE && info.files[i].ref == 2){
      found_ref2 = 1;
      break;
    }
  }
  if(!found_ref2){
    printf("[TEST 3 FAIL] File ref count was not 2 after dup()\n");
    close(fd1);
    close(fd2);
    unlink("test_dup.tmp");
    exit(1);
  }

  close(fd1);
  close(fd2);
  unlink("test_dup.tmp");
  printf("[TEST 3 PASS] dup() shared struct file and ref count verified.\n\n");
}

void
test_fork(void)
{
  printf("[TEST 4] Testing fork() and shared file between parent & child...\n");
  int fd = open("test_fork.tmp", O_CREATE | O_RDWR);
  if(fd < 0){
    printf("[TEST 4 FAIL] open failed\n");
    exit(1);
  }

  int parent_pid = getpid();
  int pid = fork();
  if(pid < 0){
    printf("[TEST 4 FAIL] fork failed\n");
    close(fd);
    unlink("test_fork.tmp");
    exit(1);
  }

  if(pid == 0){
    // Child process
    static struct sysinfo c_info;
    if(sysinfo(&c_info) < 0){
      printf("[TEST 4 FAIL] child sysinfo failed\n");
      exit(1);
    }

    int child_pid = getpid();
    int ppid_ok = 0;
    for(int i = 0; i < c_info.num_procs; i++){
      if(c_info.procs[i].pid == child_pid && c_info.procs[i].ppid == parent_pid){
        ppid_ok = 1;
        break;
      }
    }
    if(!ppid_ok){
      printf("[TEST 4 FAIL] Child's ppid did not match parent PID\n");
      exit(1);
    }

    // Check that both parent and child are referencing the open file
    int both_pids_found = 0;
    for(int i = 0; i < c_info.num_files; i++){
      if(c_info.files[i].type == S_FD_INODE && c_info.files[i].ref == 2){
        int has_parent = 0, has_child = 0;
        for(int j = 0; j < c_info.files[i].num_pids; j++){
          if(c_info.files[i].pids[j] == parent_pid) has_parent = 1;
          if(c_info.files[i].pids[j] == child_pid) has_child = 1;
        }
        if(has_parent && has_child){
          both_pids_found = 1;
          break;
        }
      }
    }
    if(!both_pids_found){
      printf("[TEST 4 FAIL] Open file does not list both parent and child PIDs\n");
      exit(1);
    }

    close(fd);
    exit(0);
  }

  // Parent process
  wait(0);
  close(fd);
  unlink("test_fork.tmp");
  printf("[TEST 4 PASS] fork() parent/child file sharing verified.\n\n");
}

void
test_pipe(void)
{
  printf("[TEST 5] Testing pipe()...\n");
  sysinfo(&info);
  int fcount_before = info.num_files;

  int p[2];
  if(pipe(p) < 0){
    printf("[TEST 5 FAIL] pipe failed\n");
    exit(1);
  }

  sysinfo(&info);
  if(info.num_files != fcount_before + 2){
    printf("[TEST 5 FAIL] Expected 2 new files for pipe, got %d -> %d\n", fcount_before, info.num_files);
    close(p[0]);
    close(p[1]);
    exit(1);
  }

  int pipe_r_found = 0, pipe_w_found = 0;
  for(int i = 0; i < info.num_files; i++){
    if(info.files[i].type == S_FD_PIPE){
      if(info.files[i].readable && !info.files[i].writable)
        pipe_r_found = 1;
      if(!info.files[i].readable && info.files[i].writable)
        pipe_w_found = 1;
    }
  }
  if(!pipe_r_found || !pipe_w_found){
    printf("[TEST 5 FAIL] Pipe read/write ends not properly reflected\n");
    close(p[0]);
    close(p[1]);
    exit(1);
  }

  close(p[0]);
  close(p[1]);
  printf("[TEST 5 PASS] Pipe file descriptors verified.\n\n");
}

void
test_zombie(void)
{
  printf("[TEST 6] Testing ZOMBIE process detection...\n");
  int pid = fork();
  if(pid < 0){
    printf("[TEST 6 FAIL] fork failed\n");
    exit(1);
  }

  if(pid == 0){
    exit(0); // Exit immediately to become zombie
  }

  // Parent sleeps briefly to allow child to exit
  sleep(5);

  sysinfo(&info);
  int zombie_found = 0;
  for(int i = 0; i < info.num_procs; i++){
    if(info.procs[i].pid == pid && info.procs[i].state == S_ZOMBIE){
      zombie_found = 1;
      break;
    }
  }

  wait(0); // Clean up child
  if(!zombie_found){
    printf("[TEST 6 FAIL] Zombie process state not detected\n");
    exit(1);
  }
  printf("[TEST 6 PASS] Zombie process successfully detected.\n\n");
}

void
test_bad_pointer(void)
{
  printf("[TEST 7] Testing invalid pointer handling...\n");
  if(sysinfo((struct sysinfo *)0) != -1){
    printf("[TEST 7 FAIL] NULL pointer should return -1\n");
    exit(1);
  }

  if(sysinfo((struct sysinfo *)0xffffffffffffffffULL) != -1){
    printf("[TEST 7 FAIL] Out-of-bounds pointer should return -1\n");
    exit(1);
  }
  printf("[TEST 7 PASS] Invalid pointer handling verified.\n\n");
}

int
main(int argc, char *argv[])
{
  printf("==========================================\n");
  printf("       STARTING SYSINFO TEST SUITE        \n");
  printf("==========================================\n\n");

  test_initial();
  test_open_close();
  test_dup();
  test_fork();
  test_pipe();
  test_zombie();
  test_bad_pointer();

  printf("==========================================\n");
  printf("     ALL SYSINFO TESTS PASSED! (7/7)      \n");
  printf("==========================================\n");
  exit(0);
}
