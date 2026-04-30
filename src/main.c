#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include "pmm.h"
#include "vmm.h"

#ifdef UNIT_TEST
#include "unit_test.h"
#endif

#define PSCI_CPU_ON_64 0xC4000003

struct secondary_boot_data {
    uint64_t stack_pointer;
    uint64_t ttbr;
    uint64_t tcr;
    uint64_t mair;
    uint64_t entry_point;
};

extern uint64_t _kernel_end;

extern void heap_init(uintptr_t start, size_t size);
extern void heap_debug();
extern void sched_init();
extern void secondary_entry();
void sched_init_secondary(int cpu_id); 

uint64_t boot_time = 0;
bool mmu_enabled = false;

//Entry points need to be a boot function similar to boot.S
int psci_cpu_on(uint64_t target_cpu, uint64_t entry_point, uint64_t context_id) {
    register uint64_t x0 asm("x0") = PSCI_CPU_ON_64;
    register uint64_t x1 asm("x1") = target_cpu;
    register uint64_t x2 asm("x2") = entry_point;
    register uint64_t x3 asm("x3") = context_id;
    
    asm volatile("hvc #0" : "=r"(x0) : "r"(x0), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}

void core_entry() {
    printf("[SMP] Core 1 online!\n");
    sched_init_secondary(1);
    sched_yield();
    while(1);
}

int main() {
    puts("Press Ctrl + A + X to exit\n");
    pmm_init((uintptr_t)&_kernel_end, (uint64_t)2 * 1024 * 1024 * 1024);
    init_vmm();
    mmu_enabled = true;
    heap_init(0x50000000, 8 * 1024 * 1024);
    sched_init();

    struct secondary_boot_data* boot_data = malloc(sizeof(struct secondary_boot_data));
    void* core1_stack = malloc(4096 * 4);
    boot_data->stack_pointer = (uint64_t)core1_stack + (4096 * 4);
    
    asm volatile("mrs %0, ttbr1_el1" : "=r"(boot_data->ttbr));
    asm volatile("mrs %0, tcr_el1"   : "=r"(boot_data->tcr));
    asm volatile("mrs %0, mair_el1"  : "=r"(boot_data->mair));
    
    boot_data->entry_point = (uint64_t)core_entry;
    psci_cpu_on(1, (uint64_t)secondary_entry, (uint64_t)boot_data);
#ifdef UNIT_TEST
    unitTestBegin(SCHED_YIELD);
#endif
    sched_yield();
    pthread_exit(NULL);
}