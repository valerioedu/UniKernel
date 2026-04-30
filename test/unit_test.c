#include "unit_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include "pmm.h"
#include "vmm.h"

#define SWITCH_ITERATIONS 10000

extern uint64_t _kernel_end;

extern void heap_init(uintptr_t start, size_t size);
extern void heap_debug();
static volatile uint64_t start_cycles = 0;

static void heap_stress() {
    while (1) {
        for (int i = 1; i < 8 * 1024 * 1023; i++) {
            void *ptr = malloc((size_t)i);
            asm volatile("yield");  // To not hammer the hardware

            if (ptr) {
                printf("Malloc Successfull! %d\n", i);
            } else {
                printf("ERROR: Allocation Failed, Halting... %d", i);
                heap_debug();
                while (1) asm volatile("yield");
            }

            free(ptr);
            asm volatile("yield");
        }
    }
}

static void heap_fragment() {
    void *ptrs[100];
    int cycle = 0;

    while (1) {
        for(int i = 0; i < 100; i++) {
            size_t alloc_size = (i % 2048) + 1;
            ptrs[i] = malloc(alloc_size);
            asm volatile("yield");

            if (!ptrs[i]) {
                printf("ERROR: Fragment Allocation Failed at index %d, Halting...\n", i);
                heap_debug();
                while (1) asm volatile("yield");
            }

            unsigned char* mem = (unsigned char*)ptrs[i];
            *mem = (unsigned char)(i & 0xFF); 
            asm volatile("yield");
        }

        for(int i = 0; i < 100; i++) {
            unsigned char* mem = (unsigned char*)ptrs[i];
            
            if (*mem != (unsigned char)(i & 0xFF)) {
                printf("ERROR: Memory corruption detected at index %d! Halting...\n", i);
                heap_debug();
                while(1) asm volatile("yield");
            }

            free(ptrs[i]);
            asm volatile("yield");
        }

        cycle++;
        printf("Fragment test cycle %d completed successfully!\n", cycle);
        asm volatile("yield");
    }
}

static inline uint64_t get_cycles() {
    uint64_t cycles;
    asm volatile("mrs %0, cntpct_el0" : "=r"(cycles));
    return cycles;
}

static void benchmark_pong(void *arg) {
    while(start_cycles == 0) {
        sched_yield();
    }

    for (int i = 0; i < SWITCH_ITERATIONS; i++) {
        sched_yield();
    }

    pthread_exit(NULL);
}

static void sched_benchmark() {
    printf("Starting Context Switch Benchmark...\n");

    pthread_create(NULL, NULL, (void* (*)(void*))benchmark_pong, NULL);
    sched_yield(); 

    start_cycles = get_cycles();

    for (int i = 0; i < SWITCH_ITERATIONS; i++) {
        sched_yield();
    }

    uint64_t end_cycles = get_cycles();
    uint64_t total_cycles = end_cycles - start_cycles;
    
    uint64_t frq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(frq));

    uint64_t total_ns = (total_cycles * 1000000000ULL) / frq;
    uint64_t ns_per_switch = total_ns / (SWITCH_ITERATIONS * 2);

    printf("--- Benchmark Results ---\n");
    printf("Total Context Switches: %d\n", SWITCH_ITERATIONS * 2);
    printf("Total Time: %lu ns\n", total_ns);
    printf("Average yield latency: %lu ns\n", ns_per_switch);
    printf("-------------------------\n");

    while(1) {
        asm volatile("wfe");
    }
}

static void *yield_thread(void *arg) {
    printf("Context switching\n");
    sched_yield();
}

static void sched_stress() {
    pthread_create(NULL, NULL, yield_thread, NULL);
    while (1) {
        printf("Context switching\n");
        sched_yield();
    }
}

[[__noreturn__]]void unitTestBegin(unitTestScenario scenario) {
    switch (scenario) {
        case HEAP_STRESS:
            heap_stress();
            break;

        case HEAP_FRAGMENT:
            heap_fragment();
            break;

        case SCHED_BENCHMARK:
            sched_benchmark();
            break;

        case SCHED_YIELD:
            sched_stress();
            break;
    }

    while (1);
}