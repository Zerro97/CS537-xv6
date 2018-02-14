#include "types.h"
#include "stat.h"
#include "user.h"

void
syscallptest(int N)
{
  int n1, n2;
  int i = 0;
  if (N < 0) {
    printf(1, "syscallptest: N cannot be negative\n");
  } else {
    n1 = getnumsyscallp();
    for (i = 0; i < N; i++) {
      uptime();   // a system call
    }
    n2 = getnumsyscallp();
    printf(1, "%d %d\n", n1, n2);
  }
}

int
main(int argc, char *argv[])
{
  if(argc != 2){
    printf(1, "usage: syscallptest N\n");
    exit();
  }

  syscallptest(atoi(argv[1]));
  exit();
}
