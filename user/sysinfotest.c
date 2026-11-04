#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/sysinfo.h"
#include "user/user.h"
#include "kernel/fcntl.h"

const char *state_names[] = {
  "UNUSED  ", "USED    ", "SLEEPING", "RUNNABLE", "RUNNING ", "ZOMBIE  "
};

const char *ftype_names[] = {
  "FD_NONE  ", "FD_PIPE  ", "FD_INODE ", "FD_DEVICE"
};

// Khai báo toàn cục để không làm tràn stack 4KB của user space
struct sysinfo info;

void print_sysinfo(const char *title) {
  if (sysinfo(&info) < 0) {
    printf("sysinfo failed!\n");
    exit(1);
  }

  printf("\n================ %s ================\n", title);
  printf(">> Tong so file dang mo trong he thong: %d\n", info.nopenfiles);
  for (int i = 0; i < info.nopenfiles; i++) {
    struct file_info *f = &info.files[i];
    printf("   + ftable[%d]: type=%s | mode=%c%c | ref=%d | pids=[",
           f->f_idx, ftype_names[f->type],
           f->readable ? 'r' : '-', f->writable ? 'w' : '-', f->ref);
    for (int j = 0; j < f->npids; j++) {
      printf("%d%s", f->pids[j], (j == f->npids - 1) ? "" : ",");
    }
    printf("]\n");
  }

  printf(">> Tong so tien trinh dang hoat dong: %d\n", info.nprocs);
  for (int i = 0; i < info.nprocs; i++) {
    struct proc_info *p = &info.procs[i];
    printf("   + PID=%d (Cha: %d) | Trang thai=%s | Sched=%ld | File_mo(%d)=[",
           p->pid, p->ppid, state_names[p->state], p->sched_count, p->nfiles);
    for (int j = 0; j < p->nfiles; j++) {
      printf("%d%s", p->open_files[j], (j == p->nfiles - 1) ? "" : ",");
    }
    printf("]\n");
  }
}

// Test trường hợp truyền địa chỉ con trỏ lỗi (kế thừa từ bài test MIT)
void test_bad_address() {
  printf("\n>> Kiem tra truyen dia chi con tro loi (Bad address)...\n");
  if (sysinfo((struct sysinfo *) 0xeaeb0b5b00002f5e) != -1) {
    printf("FAIL: sysinfo chap nhan dia chi rac!\n");
    exit(1);
  }
  printf("   => Ket qua: Xu ly an toan, tra ve -1 thanh cong!\n");
}

int main(void) {
  printf("=== BAT DAU KIEM THU SYSINFO CHO PROJECT 1 ===\n");

  // Kịch bản 0: Kiểm tra an toàn địa chỉ
  test_bad_address();

  // Kịch bản 1: Trạng thái ban đầu
  print_sysinfo("KICH BAN 1: HE THONG BAN DAU");

  // Kịch bản 2: Kiểm tra open() và dup()
  int fd1 = open("README", O_RDONLY);
  int fd2 = dup(fd1);
  print_sysinfo("KICH BAN 2: SAU KHI OPEN('README') VA DUP()");
  close(fd1);
  close(fd2);

  // Kịch bản 3: Kiểm tra fork(), pipe() va scheduling events
  int pfd[2];
  pipe(pfd);
  int pid = fork();
  if (pid == 0) {
    // Tiến trình con ngủ một lúc để tạo thêm sự kiện lập lịch
    sleep(2);
    close(pfd[0]);
    close(pfd[1]);
    exit(0);
  } else {
    sleep(1);
    print_sysinfo("KICH BAN 3: KHI CO TIEN TRINH CON DANG CHAY & PIPE");
    close(pfd[0]);
    close(pfd[1]);
    wait(0);
  }

  // Kịch bản 4: Sau khi tiến trình con kết thúc
  print_sysinfo("KICH BAN 4: SAU KHI TIEN TRINH CON THOAT");

  printf("\n=== TAT CA CAC KICH BAN KIEM THU DEU HOAN TAT! ===\n");
  exit(0);
}