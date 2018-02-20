#ifndef _THREAD_LOCK_H
#define _THREAD_LOCK_H

struct lock_t {
  uint locked;       // Is the lock held?
};

typedef void (*thread_func)(void*);

#endif
