#include "types.h"
#include "stat.h"
#include "user.h"


int
main(int argc, char *argv[])
{
  int* a = (int*)shmgetat(1, 1);
  a[1] = 100;
  printf(1, "a[1] = %d\n", a[1]);
  int c = shm_refcount(1);
  printf(1, "key=%d, count = %d\n", 1, c);
  exit();
}
