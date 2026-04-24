#include <errno.h>
#include <stdbool.h>
#include <pthread.h>

int pthread_spin_init(pthread_spinlock_t *lock, int pshared) {
    __atomic_store_n(lock, 0, __ATOMIC_RELAXED);
    return 0;
}

int pthread_spin_lock(pthread_spinlock_t *lock) {
    while (1) {
        pthread_spinlock_t expected = 0;
        
        if (__atomic_compare_exchange_n(lock, &expected, 1, true, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)) {
            return 0; 
        }

        while (__atomic_load_n(lock, __ATOMIC_RELAXED) == 1) {
            asm volatile("yield" ::: "memory");
        }
    }
}

int pthread_spin_trylock(pthread_spinlock_t *lock) {
    pthread_spinlock_t expected = 0;
    if (!__atomic_compare_exchange_n(lock, &expected, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE)) {
        return EBUSY;
    }

    return 0;
}

int pthread_spin_unlock(pthread_spinlock_t *lock) {
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
    return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *lock) {
    pthread_spinlock_t expected = 0;
    if (__atomic_load_n(lock, __ATOMIC_ACQUIRE) != 0) {
        return EBUSY;
    }

    return 0;
}