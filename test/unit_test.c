#include "unit_test.h"

#include <stdio.h>
#include <stdlib.h>
#include "pmm.h"
#include "vmm.h"

extern uint64_t _kernel_end;

extern void heap_init(uintptr_t start, size_t size);
extern void heap_debug();

void heap_stress() {
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

void heap_fragment() {
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

[[__noreturn__]]void unitTestBegin(unitTestScenario scenario) {
    switch (scenario) {
        case HEAP_STRESS:
            heap_stress();
            break;

        case HEAP_FRAGMENT:
            heap_fragment();
            break;
    }

    while (1);
}