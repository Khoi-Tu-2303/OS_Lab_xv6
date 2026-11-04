#ifndef _SYSINFO_H_
#define _SYSINFO_H_

#ifndef NPROC
#define NPROC 64
#endif

#ifndef NFILE
#define NFILE 100
#endif

#ifndef NOFILE
#define NOFILE 16
#endif

// File descriptor types (corresponds to enum in kernel/file.h)
#define S_FD_NONE   0
#define S_FD_PIPE   1
#define S_FD_INODE  2
#define S_FD_DEVICE 3

// Process states (corresponds to enum procstate in kernel/proc.h)
#define S_UNUSED   0
#define S_USED     1
#define S_SLEEPING 2
#define S_RUNNABLE 3
#define S_RUNNING  4
#define S_ZOMBIE   5

struct file_info {
  short type;          // S_FD_PIPE, S_FD_INODE, S_FD_DEVICE
  char readable;       // 1 if readable, 0 otherwise
  char writable;       // 1 if writable, 0 otherwise
  int ref;             // reference count
  int num_pids;        // number of processes referencing this file
  int pids[NPROC];     // list of process IDs
};

struct proc_info {
  int pid;             // Process ID
  int state;           // S_USED, S_SLEEPING, S_RUNNABLE, S_RUNNING, S_ZOMBIE
  int ppid;            // Parent Process ID
  int sched_count;     // Number of scheduling events
  int num_open_files;  // Number of open files
  int open_files[NOFILE]; // List of open file descriptors
};

struct sysinfo {
  int num_files;
  struct file_info files[NFILE];
  int num_procs;
  struct proc_info procs[NPROC];
};

#endif
