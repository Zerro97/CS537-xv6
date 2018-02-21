#include "types.h"
#include "stat.h"
#include "pstat.h"
#include "user.h"

int count;
struct lock_t mutex;

void
count_num(void* arg)
{
  printf(1, "enter count_num\n");
  int i;
  int n = *(int*)arg;
  for (i = 0; i < n; i++) {
    lock_acquire(&mutex);   
    count++;
    lock_release(&mutex);
  }
  printf(1, "in thread count=%d\n", count);
  exit();
}

void
sleep_func(void* arg)
{
  printf(1, "enter sleep_func\n");
  int ticks = *(int*)arg;
  sleep(ticks);
  exit();
}

void
test_single_thread(void)
{
  printf(1, "test_single_thread\n");
  int i;
  char* c;
  int pid;

  i = 1000;
  c = (char*)malloc(1000);
  c[1] = 1;

  printf(1, "count_num address: %x\n", count_num);
  pid = thread_create(count_num, &i);
  
  printf(1, "create thread tid = %d\n", pid);
  thread_join();
  printf(1, "thread exit. count=%d\n", count);
  free(c);
}

// some clone will fail.
void
test_n_thread(int n)
{
  printf(1, "test_n_thread\n");
  int pid, i;
  int n_th = 0;
  int ticks = 100;
  for (i = 0; i < n; i++) {
    pid = thread_create(sleep_func, &ticks);
    if (pid < 0) {
      printf(1, "create thread failed\n");
    } else {
      printf(1, "create thread (id=%d)\n", pid);
      n_th++;
    }
  }
  // create another proc
  pid = fork();
  if (pid < 0) {
    printf(1, "create process failed\n");
  } else if (pid == 0) {
    printf(1, "fork: child\n");
    exit();
  } else {
    printf(1, "create process (id=%d)\n", pid);
    if (wait() != pid) {
      printf(2, "test_n_thread: wait unexpected process (expected=%d)\n", pid);
    }
  }
  
  // join
  for (i = 0; i < n_th; i++) {
    pid = thread_join();
    printf(1, "pid = %d join \n", pid);
  }
}

// mixed fork and clone
//    fork (1)
//     |       \  # 
//     1       |  clone (2)
//             |      \  #
//             2    fork(3)    
//                    |     \  #
//                    3     4

void
call_fork(void* arg)
{
  int pid = fork();
  int fork_num = *(int*)arg;
  if (pid < 0) {
    printf(2, "call_fork(%d): failed\n", fork_num);
  } else if (pid == 0) {
    printf(1, "call_fork(%d): child, sleep\n", fork_num);
    sleep(10);
    exit();
  } else {
    printf(1, "call_fork(%d): parent\n", fork_num);
    if (wait() != pid) {
      printf(2, "wait expected: %d\n", pid);
    }
    printf(1, "child exits\n");
    exit();
  }
}
void
test_mix_fork_clone(void)
{
  printf(1, "test_mix_fork_clone\n");
  struct pstat ps;
  int pid1, pid2;
  int fn = 3;
  pid1 = fork();
  if (pid1 < 0) {
    printf(2, "test_mix_fork_clone: fork (1) failed\n");
  } else if (pid1 == 0) {
    printf(1, "test_mix_fork_clone: fork (1) child\n");
    pid2 = thread_create(call_fork, &fn);
    if (pid2 < 0) {
      printf(2, "test_mix_fork_clone: clone (2) failed\n");
    } else if (pid2 > 0) {
      if (thread_join() != pid2) {
        printf(2, "test_mix_fork_clone: join (3) expected pid=%d\n", pid2);
      } else {
        printf(1, "join thread (pid=%d)\n", pid2);
      }
    }
  } else {
    if (wait() != pid1) {
      printf(2, "test_mix_fork_clone: wait (2) expected pid=%d\n", pid1);
    }
    // get the process table
    printf(2, "Final process table:\n");
    if (getpinfo(&ps) < 0) {
      printf(2, "getpinfo: cannot get process information");
    }
    print_proc_info(&ps, 1);
  }
}



void
test_lock(void)
{
  int nth = 10;
  int i, n_count = 100000;
  printf(1, "test_lock\n");
  count = 0;
  for (i = 0; i < nth; i++) {
    thread_create(count_num, &n_count);
  }
  for (i = 0; i < nth; i++) {
    thread_join();
  }
  if (count != n_count * nth) {
    printf(2, "test_lock : wrong count=%d, expected=%d\n", count, n_count * nth);
  }
}

struct shm_comm {
  int key;
  char* msg;
};

void
shmgetat_routine(void *arg)
{
  struct shm_comm* com = (struct shm_comm*)arg;
  int key = com->key;
  char* msg = com->msg;
  char* shmem = shmgetat(key, 2);
  strcpy(shmem, msg);
  exit();
}

//      fork (1)
//       |       \ #
//              clone (2)
//                |    \ #

void
test_shmgetat(void)
{
  int pid1, pid2;
  int key = 0;
  char* shmem = shmgetat(key, 1);
  struct shm_comm com = {key, "hello grandpa"};
  printf(1, "test_shmgetat\n");
  pid1 = fork();
  if (pid1 < 0) {
    printf(2, "test_shmgetat: fork (1) failed\n");
  } else if (pid1 == 0) {
    printf(1, "test_shmgetat: fork (1) child\n");
    pid2 = thread_create(shmgetat_routine, &com);
    if (pid2 < 0) {
      printf(2, "test_shmgetat: clone (2) failed\n");
      exit();
    } else if (pid2 > 0) {
      if (thread_join() != pid2) {
        printf(2, "test_shmgetat: pid=%d did not exit\n", pid2);
      }
      if (shm_refcount(key) != 2) {
        printf(2, "test_shmgetat: clone (2) wrong ref count for key=%d: %d, expected: 2\n", key, shm_refcount(key)); 
      }
      exit();
    }
  } else {
    if(wait() != pid1) {
      printf(2, "test_shmgetat: pid=%d did not exit\n", pid1);
    }
    if (strcmp(com.msg, shmem) != 0) {
      printf(2, "test_shmgetat: shm communication failed: com.msg=%s, shmem=%s\n", com.msg, shmem);
    }

    if (shm_refcount(key) != 1) {
      printf(2, "test_shmgetat: fork (1) wrong ref count for key=%d: %d, expected: 1\n", key, shm_refcount(key)); 
    }
  }
}

void
exec_func(void* arg)
{
  struct shm_comm* com = (struct shm_comm*)arg;
  char* shm = shmgetat(com->key, 1);
  char* argv[2] = {"echo", 0};
  if (strcmp(shm, com->msg) != 0) {
    printf(2, "test_mix_clone_exec: shm communication failed: com->msg=%s, shm=%s\n", com->msg, shm);
  }
  if (exec(argv[0], argv) < 0) {
    printf(2, "test_mix_clone_exec: failed exec\n");
  }
  exit();
}

void
test_mix_clone_exec(void)
{
  struct shm_comm com = {0, "hello text_mix_clone_exec"};
  char* shm = shmgetat(com.key, 1);
  int pid;
  strcpy(shm, com.msg);
  pid = thread_create(exec_func, &com);
  if (pid < 0) {
    printf(2, "test_mix_clone_exec: thread_create failed\n");
  } else if (pid > 0) {
    sleep(10);    // let exec really happen
    if (wait() != pid) {
      printf(2, "test_mix_clone_exec: pid=%d did not exit\n", pid);
    }
  }
  // can still access shm
  if (strcmp(shm, com.msg) != 0) {
    printf(2, "test_mix_clone_exec: shm communication failed: com.msg=%s, shm=%s\n", com.msg, shm);
  }
}

int
main(int argc, char *argv[])
{
  lock_init(&mutex);
  // test_single_thread();
  // test_n_thread(64);
  // test_mix_fork_clone();
  // test_lock();
  //test_shmgetat();
  test_mix_clone_exec();
  exit();
}
