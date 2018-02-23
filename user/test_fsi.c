// test fs checksum
#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"

char*
gen_str(char c, int repeat)
{
  char* s = (char*)malloc(repeat + 1);
  char* ptr;
  for (ptr = s; ptr < s + repeat; ptr++) {
    *ptr = c;
    c++;
  }
  *ptr = '\0';
  return s;
}

void
test_write_then_read1(int n)
{
  int fd, nw, nr;
  char* path = "tmp_chk";
  char* s = gen_str('a', n);
  char* buf = (char*)malloc(n + 1);
  if ((fd = open(path, O_WRONLY | O_CHECKED | O_CREATE)) < 0) {
    printf(2, "test_write_then_read1: cannot open for write %s\n", path);
  }
  nw = write(fd, s, n);
  if (nw != n) {
    printf(2, "test_write_then_read1: write %d, expected %d\n", nw, n);
  }
  close(fd);
  if ((fd = open(path, O_RDONLY | O_CHECKED)) < 0) {
    printf(2, "test_write_then_read1: cannot open for read %s\n", path);
  }
  nr = read(fd, buf, n);
  if (nr != n) {
    printf(2, "test_write_then_read1: read %d, expected %d\n", nr, n);
  }
  if (strcmp(s, buf) != 0) {
    printf(2, "test_write_then_read1: s != buf\n");
  }
  close(fd);
}


int
main(int argc, char *argv[])
{
  test_write_then_read1(1000);
  exit();
}
