#include <stdio.h>
#include <stdlib.h>
#include "pmm.h"
#include "vmm.h"

extern uint64_t _kernel_end;

int main() {
    puts("Press Ctrl + A + X to exit\n");
    int ret = printf("First print!\n");
    printf("Returned: %d\n", ret);
    printf("String and hex test: %s %X\n", "Passed!", 0xFFFF);
    pmm_init((uintptr_t)&_kernel_end, (uint64_t)2 * 1024 * 1024 * 1024);
    init_vmm();
}