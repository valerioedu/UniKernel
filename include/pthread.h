#ifndef PTHREAD_H
#define PTHREAD_H

#include <stdint.h>

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#ifndef __SIZE_T
#define __SIZE_T
typedef typeof(sizeof(0)) size_t;
#endif

typedef _Atomic(unsigned int) pthread_spinlock_t;
typedef void *pthread_t;

typedef struct {
    int detachstate;     // Joinable or Detached
    size_t stacksize;    // Custom stack size
    void *stackaddr;     // Provide your own stack memory
    int inheritsched;    // Inherit scheduling properties from parent?
    int schedpolicy;     // FIFO, RR, etc.
    int schedparam;      // Priority
    
    // Add an internal flag to ensure the attribute was initialized via pthread_attr_init
    int is_initialized; 
} pthread_attr_t;

int pthread_spin_init(pthread_spinlock_t *lock, int pshared);
int pthread_spin_lock(pthread_spinlock_t *lock);
int pthread_spin_trylock(pthread_spinlock_t *lock);
int pthread_spin_unlock(pthread_spinlock_t *lock);
int pthread_spin_destroy(pthread_spinlock_t *lock);
int pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr, void *(*start_routine)(void*), void *restrict arg);
[[noreturn]] void pthread_exit(void *value_ptr);

int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
int pthread_attr_getstacksize(const pthread_attr_t *restrict attr, size_t *restrict stacksize);
int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize);
int pthread_attr_getstack(const pthread_attr_t *restrict attr, void **restrict stackaddr, size_t *restrict stacksize);
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);
int pthread_attr_getschedpolicy(const pthread_attr_t *restrict attr, int *restrict policy);
int pthread_attr_setschedparam(pthread_attr_t *restrict attr, const int *restrict param);
int pthread_attr_getschedparam(const pthread_attr_t *restrict attr, int *restrict param);

#endif