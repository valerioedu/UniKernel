#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include "pmm.h"
#include "vmm.h"

#ifdef UNIT_TEST
#include "unit_test.h"
#endif

#define PSCI_CPU_ON_64 0xC4000003

extern uint64_t _kernel_end;

extern void heap_init(uintptr_t start, size_t size);
extern void heap_debug();
extern void sched_init();

//Entry points need to be a boot function similar to boot.S
int psci_cpu_on(uint64_t target_cpu, uint64_t entry_point, uint64_t context_id) {
    register uint64_t x0 asm("x0") = PSCI_CPU_ON_64;
    register uint64_t x1 asm("x1") = target_cpu;
    register uint64_t x2 asm("x2") = entry_point;
    register uint64_t x3 asm("x3") = context_id;
    
    asm volatile("hvc #0" : "=r"(x0) : "r"(x0), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}

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