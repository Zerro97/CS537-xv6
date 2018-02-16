#include "types.h"
#include "pstat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int verbose = 0;
  struct pstat ps;

  if(argc == 2){
    if (strcmp(argv[1], "-v") == 0) {
      verbose = 1;
    } else {
      printf(2, "Usage: getpinfo [-v]\n");
      exit();
    }
  } else if (argc > 2) {
    printf(2, "Usage: getpinfo [-v]\n");
    exit();
  }

  if (getpinfo(&ps) < 0) {
    printf(2, "getpinfo: cannot get process information");
  }
  print_proc_info(&ps, verbose);
  exit();
}
