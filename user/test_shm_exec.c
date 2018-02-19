#include "types.h"
#include "stat.h"
#include "user.h"

#define SHM_SIZE 4096U
#define MAX_MEM  0xA0000U

int
main(int argc, char *argv[])
{
  int *a = shmgetat(0, 3); 
  if ((uint)a != MAX_MEM - 3 * SHM_SIZE) {
    printf(2, "test_shm_exec: failed. address of a: %x, expected: %x\n", a, MAX_MEM - 3 * SHM_SIZE);
  }
  exit();
}
