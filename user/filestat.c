#include "types.h"
#include "stat.h"
#include "user.h"

char buf[512];

void
print_stat(struct stat* fst)
{
  printf(1, "type: %d, dev: %d, inode number: %d, nlink: %d, size: %d, checksum: %x\n",
      fst->type, fst->dev, fst->ino, fst->nlink, fst->size, fst->checksum);
}

int
main(int argc, char *argv[])
{
  int fd;
  struct stat fst;

  if(argc != 2){
    printf(1, "usage: filestat filename\n");
    exit();
  }

  if((fd = open(argv[1], 0)) < 0){
    printf(1, "cat: cannot open %s\n", argv[1]);
    exit();
  }
  fstat(fd, &fst);
  print_stat(&fst);
  exit();
}
