// P2b benchmark the scheduler

#include "types.h"
#include "pstat.h"
#include "user.h"

#define MAX_PROC 5000

// #define VERBOSE

struct ptime {
  int pid[MAX_PROC];
  int ticks_start[MAX_PROC];
  int ticks_end[MAX_PROC];
  int nprocs;
};

static struct ptime ptime_rcd;

void
ptime_rcd_init(struct ptime* prcd) {
  prcd->nprocs = 0;
}

void
ptime_rcd_add_start(struct ptime* prcd, int pid, int start_time) {
  prcd->pid[prcd->nprocs] = pid;
  prcd->ticks_start[prcd->nprocs] = start_time;
  prcd->nprocs++;
}

void
ptime_rcd_add_end(struct ptime* prcd, int pid, int end_time) {
  int i = 0;
  for (i = 0; i < prcd->nprocs; i++) {
    if (prcd->pid[i] == pid) {
      prcd->ticks_end[i] = end_time;
      return;
    }
  }
  printf(2, "ptime_rcd_add_end failed to find the pid\n");
}

void
ptime_rcd_print(struct ptime* prcd) {
  int avg = 0;
  int turnaround = 0;
  int i = 0;
  printf(1, "process summary\n");
  for (i = 0; i < prcd->nprocs; i++) {
    turnaround = prcd->ticks_end[i] - prcd->ticks_start[i];
    avg += turnaround;
    printf(1, "pid=%d, start_ticks=%d, end_ticks=%d, turnaround=%d\n",
        prcd->pid[i], prcd->ticks_start[i], prcd->ticks_end[i], turnaround);
  }
  printf(1, "average turnaround time=%d\n", avg / prcd->nprocs);
}

void
empty_loop(int n) {
  while (0 < n--) {
    (void)(n * n * n);
  }
}

void
periodic_sleep(int period, int n) {
  while (0 < n--) {
    sleep(period);
  }
}

void
read_then_write(char* path) {   // in fact only read
  char buf[512];
  int n;
  int fd = open(path, 0);
  if (fd < 0) {
    printf(2, "cat: cannot open %s\n", path);
    exit();
  }

  while((n = read(fd, buf, sizeof(buf))) > 0);
  // write(1, buf, n);
  if(n < 0){
    printf(2, "cat: read error\n");
    exit();
  }
}

int
create_new_loop_proc(int n) {
  int pid = fork();
  if (pid < 0) {
    printf(2, "scheduler_benchmark : create_new_loop_proc failed.\n");
  } else if (pid == 0) {
    empty_loop(n);
    exit();
  }
  return pid;
}

int
create_new_io_proc(char *path) {
  int pid = fork();
  if (pid < 0) {
    printf(2, "scheduler_benchmark : create_new_io_proc failed.\n");
  } else if (pid == 0) {
    read_then_write(path);
    exit();
  }
  return pid;
}

int
create_new_sleep_proc(int period, int n) {
  int pid = fork();
  if (pid < 0) {
    printf(2, "scheduler_benchmark : create_new_sleep_proc failed.\n");
  } else if (pid == 0) {
    periodic_sleep(period, n);
    exit();
  }
  return pid;
}

// n concurrent processes
void
benchmark1(int nprocs, int nloops, int period, int repeat) {
  printf(1, "benchmark1: num processes=%d, num loops=%d\n", nprocs, nloops);
  ptime_rcd_init(&ptime_rcd);
  struct pstat ps;
  int pi = 0;
  int pid = 0;
  int ticks = 0;
  int failed_procs = 0;
  for (pi = 0; pi < nprocs; pi++) {
    pid = create_new_loop_proc(nloops);
    if (pid < 0) {
      failed_procs++;
    } else {
      ticks = uptime();
      ptime_rcd_add_start(&ptime_rcd, pid, ticks);
#ifdef VERBOSE
      printf(1, "create pid=%d at %d\n", pid, ticks);
#endif
    }
  }
   
#ifdef VERBOSE
  int t = 0;
  // print the process table every period ticks
  for (t = 0; t < repeat; t++) {
    if (getpinfo(&ps) < 0) {
      printf(2, "getpinfo: cannot get process information");
    }
    print_proc_info(&ps, 1);
    sleep(period);
  }
#endif

  for (pi = 0; pi < nprocs - failed_procs; pi++) {
    pid = wait();
    ticks = uptime();
    if(pid < 0) {
      printf(1, "wait stopped early\n");
      continue;
    }
    ptime_rcd_add_end(&ptime_rcd, pid, ticks);
  }
  printf(2, "all children terminated\n");
  if (getpinfo(&ps) < 0) {
    printf(2, "getpinfo: cannot get process information");
  }
  print_proc_info(&ps, 1);
  ptime_rcd_print(&ptime_rcd);
}

// some short processes arrives, preempting long processes
void
benchmark2(int longprocs, int longloops, int shortprocs, int shortloops,
    int period, int repeat) {
  printf(1, "benchmark2: num processes=%d longs, %d shorts, num loops=%d for long, %d for short\n",
      longprocs, shortprocs, longloops, shortloops);
  ptime_rcd_init(&ptime_rcd);
  struct pstat ps;
  int pi = 0;
  int pid = 0;
  int ticks = 0;
  int failed_procs = 0;
  // create long loops
  for (pi = 0; pi < longprocs; pi++) {
    pid = create_new_loop_proc(longloops);
    if (pid < 0) {
      failed_procs++;
    } else {
      ticks = uptime();
      ptime_rcd_add_start(&ptime_rcd, pid, ticks);
#ifdef VERBOSE
      printf(1, "create pid=%d at %d\n", pid, ticks);
#endif
    }
  }
#ifdef VERBOSE
  int t = 0;
  // print the process table every period ticks
  for (t = 0; t < repeat; t++) {
    if (getpinfo(&ps) < 0) {
      printf(2, "getpinfo: cannot get process information");
    }
    print_proc_info(&ps, 1);
    sleep(period);
  }
#else
  sleep(period * repeat);
#endif
  // create short loops
  for (pi = 0; pi < shortprocs; pi++) {
    pid = create_new_loop_proc(shortloops);
    if (pid < 0) {
      failed_procs++;
    } else {
      ticks = uptime();
      ptime_rcd_add_start(&ptime_rcd, pid, ticks);
#ifdef VERBOSE
      printf(1, "create pid=%d at %d\n", pid, ticks);
#endif
    }
  }
#ifdef VERBOSE
  for (t = 0; t < repeat; t++) {
    if (getpinfo(&ps) < 0) {
      printf(2, "getpinfo: cannot get process information");
    }
    print_proc_info(&ps, 1);
    sleep(period);
  }
#endif
  for (pi = 0; pi < longprocs + shortprocs - failed_procs; pi++) {
    pid = wait();
    ticks = uptime();
    if(pid < 0) {
      printf(1, "wait stopped early\n");
      continue;
    }
    ptime_rcd_add_end(&ptime_rcd, pid, ticks);
  }
  printf(2, "all children terminated\n");
  if (getpinfo(&ps) < 0) {
    printf(2, "getpinfo: cannot get process information");
  }
  print_proc_info(&ps, 1);
  ptime_rcd_print(&ptime_rcd);
}

// CPU intensity + I/O intensity + periodically sleeping
void
benchmark3(int nprocs, int nloops, int t_sleep, int repeat, char* path) {
  ptime_rcd_init(&ptime_rcd);
  struct pstat ps;
  int pi = 0;
  int pid = 0;
  int ticks = 0;
  for (pi = 0; pi < nprocs; pi++) {
    pid = create_new_loop_proc(nloops);
    ticks = uptime();
    ptime_rcd_add_start(&ptime_rcd, pid, ticks);
#ifdef VERBOSE
    printf(1, "create pid=%d at %d\n", pid, ticks);
#endif
    pid = create_new_sleep_proc(t_sleep, repeat);
    ticks = uptime();
    ptime_rcd_add_start(&ptime_rcd, pid, ticks);
#ifdef VERBOSE
    printf(1, "create pid=%d at %d\n", pid, ticks);
#endif
    pid = create_new_io_proc(path);
    ticks = uptime();
    ptime_rcd_add_start(&ptime_rcd, pid, ticks);
#ifdef VERBOSE
    printf(1, "create pid=%d at %d\n", pid, ticks);
#endif
  }
   
#ifdef VERBOSE
  int t = 0;
  // print the process table every period ticks
  for (t = 0; t < repeat; t++) {
    if (getpinfo(&ps) < 0) {
      printf(2, "getpinfo: cannot get process information");
    }
    print_proc_info(&ps, 1);
    sleep(period);
  }
#endif

  for (pi = 0; pi < 3 * nprocs; pi++) {
    pid = wait();
    ticks = uptime();
    if(pid < 0) {
      printf(1, "wait stopped early\n");
      exit();
    }
    ptime_rcd_add_end(&ptime_rcd, pid, ticks);
  }
  printf(2, "all children terminated\n");
  if (getpinfo(&ps) < 0) {
    printf(2, "getpinfo: cannot get process information");
  }
  print_proc_info(&ps, 1);
  ptime_rcd_print(&ptime_rcd);

}

int
main(int argc, char *argv[])
{
  benchmark1(5, 100000000, 80, 5);
  benchmark1(63, 100000000, 200, 5);
  benchmark2(10, 100000000, 10, 1000000, 80, 2);
  benchmark3(3, 100000000, 10, 10, "README");
  exit();
}
