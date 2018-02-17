#include "types.h"
#include "stat.h"
#include "user.h"


int
main(int argc, char *argv[])
{
  shmgetat(1, 1);
  shm_refcount(1);
  exit();
}
