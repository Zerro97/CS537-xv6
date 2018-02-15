#include "types.h"
#include "pstat.h"
#include "user.h"

void print_proc_info(struct pstat* ps, int verbose) {
  int i = 0;
  char* state_str;
  int count = 0;
  
  for (i = 0; i < NPROC; i++) {
    switch(ps->state[i]) {
      case UNUSED:
        state_str = "unused";
        break;
      case EMBRYO:
        state_str = "embryo";
        break;
      case SLEEPING:
        state_str = "sleeping";
        break;
      case RUNNABLE:
        state_str = "runnable";
        break;
      case RUNNING:
        state_str = "running";
        break;
      case ZOMBIE:
        state_str = "zombie";
        break;
      default:
        state_str = "unknown";
    }
    if (ps->inuse[i]) {
      count++;
      if (verbose) {
        printf(1, "pid=%d, state=%s, priority=%d, ticks=%d\n",
            ps->pid[i], state_str, ps->priority[i], 
            ps->ticks[i][ps->priority[i]]);
      }
    }
  }
  printf(1, "Total number of processes: %d\n", count);
}


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
