#ifndef _SYSINFO_H_
#define _SYSINFO_H_

#include "types.h"
#include "param.h"

// Thông tin chi tiết của 1 file đang mở trong hệ thống (ftable)
struct file_info {
  int f_idx;              // Vị trí của file trong mảng ftable.file[]
  int type;               // Loại file: FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE
  char readable;          // Quyền đọc
  char writable;          // Quyền ghi
  int ref;                // Biến đếm tham chiếu (reference count)
  int pids[NPROC];        // Danh sách PID các tiến trình đang mở file này
  int npids;              // Số lượng tiến trình trong mảng pids
};

// Thông tin chi tiết của 1 tiến trình đang hoạt động (state != UNUSED)
struct proc_info {
  int pid;                // Mã PID của tiến trình
  int state;              // Trạng thái hiện tại
  int ppid;               // PID của tiến trình cha
  uint64 sched_count;     // Số lần tiến trình được CPU lập lịch chạy
  int open_files[NOFILE]; // Danh sách các file mà tiến trình đang mở (lưu f_idx)
  int nfiles;             // Số lượng file đang mở
};

// Cấu trúc tổng hợp sysinfo theo đúng yêu cầu Mục 4.2 của đồ án
struct sysinfo {
  uint64 freemem;                // (Tùy chọn) Giữ lại bộ nhớ trống nếu muốn
  int nopenfiles;                // Tổng số file đang mở trên toàn hệ thống
  struct file_info files[NFILE]; // Danh sách chi tiết từng file

  int nprocs;                    // Số lượng tiến trình khác UNUSED
  struct proc_info procs[NPROC]; // Danh sách chi tiết từng tiến trình
};

#endif