#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "usage: vmprint command [args...]\n");
    exit(1);
  }
  execpt(argv[1], argv + 1);
  fprintf(2, "vmprint: exec %s failed\n", argv[1]);
  exit(1);
}
