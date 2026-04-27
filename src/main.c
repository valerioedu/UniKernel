#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include "pmm.h"
#include "vmm.h"

#ifdef UNIT_TEST
#include "unit_test.h"
#endif

extern uint64_t _kernel_end;

extern void heap_init(uintptr_t start, size_t size);
extern void heap_debug();
extern void sched_init();

int main() {
    puts("Press Ctrl + A + X to exit\n");
    pmm_init((uintptr_t)&_kernel_end, (uint64_t)2 * 1024 * 1024 * 1024);
    init_vmm();
    heap_init(0x50000000, 8 * 1024 * 1024);
#ifdef UNIT_TEST
    unitTestBegin(HEAP_STRESS);
#endif
    sched_init();
    sched_yield();
    pthread_exit(NULL);
}