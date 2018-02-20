#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"

char*
strcpy(char *s, char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint
strlen(char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

void*
memset(void *dst, int c, uint n)
{
  stosb(dst, c, n);
  return dst;
}

char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

void*
memmove(void *vdst, void *vsrc, int n)
{
  char *dst, *src;
  
  dst = vdst;
  src = vsrc;
  while(n-- > 0)
    *dst++ = *src++;
  return vdst;
}

void
print_proc_info(struct pstat* ps, int verbose)
{
  int i = 0;
  char* state_str;
  int count = 0;
  
  printf(1, "current ticks=%d\n", uptime());
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
        printf(1, "pid=%d, state=%s, priority=%d, ticks=[%d, %d, %d, %d]\n",
            ps->pid[i], state_str, ps->priority[i], 
            ps->ticks[i][0], ps->ticks[i][1], ps->ticks[i][2], ps->ticks[i][3]);
      }
    }
  }
  printf(1, "Total number of processes: %d\n", count);
}


// p4b
int
thread_create(thread_func start_routine, void *arg)
{
  void *ptr_s, *stack;
  uint page_size = 4096U;
  ptr_s = malloc(page_size * 2);
  if (ptr_s == 0) {
    printf(2, "thread_create: no enough memory for stack");
    return -1;
  }
  stack = (void*)(((uint)ptr_s + page_size) & ~0xFFF);
  printf(1, "thread_create stack loc: stack %x, upper bound: %x, size: %d\n",
      stack, (uint)ptr_s + 2 * page_size, (uint)ptr_s + 2 * page_size - (uint)stack);
  return clone(start_routine, arg, stack);
}

void
lock_acquire(struct lock_t *lock)
{
}

void lock_release(struct lock_t *lock)
{
}

void lock_init(struct lock_t *lock)
{
  lock->locked = 0;
}

