#ifndef PTHREAD_H
#define PTHREAD_H

#include <stdint.h>

typedef _Atomic uint32_t pthread_spinlock_t;

int pthread_spin_init(pthread_spinlock_t *lock, int pshared);
int pthread_spin_lock(pthread_spinlock_t *lock);
int pthread_spin_trylock(pthread_spinlock_t *lock);
int pthread_spin_unlock(pthread_spinlock_t *lock);
int pthread_spin_destroy(pthread_spinlock_t *lock);

#endif