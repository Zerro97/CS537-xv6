#include "types.h"
#include "stat.h"
#include "user.h"

int count;

void
count_num(void* arg)
{
  printf(1, "enter count_num\n");
  int i;
  int n = *(int*)arg;
  for (i = 0; i < n; i++)
    count++;
  printf(1, "in thread count=%d\n", count);
}

int
main(int argc, char *argv[])
{
  int i;
  char* c;
  int pid;
  count = 0;

  i = 1000;
  c = (char*)malloc(1000);
  c[1] = 1;

  pid = thread_create(count_num, &i);
  
  printf(1, "create thread tid = %d\n", pid);

  exit();
}
