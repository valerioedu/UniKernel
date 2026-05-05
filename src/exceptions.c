#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define u8  uint8_t
#define u32 uint32_t
#define u64 uint64_t

typedef struct {
    uint64_t x[31];     // X0 - X30 (Offset 0 - 240)
    uint64_t _padding;  // Padding at offset 248 
    uint64_t sp_el0;    // Offset 256
    uint64_t elr_el1;   // Offset 264 (The PC)
    uint64_t spsr_el1;  // Offset 272 (The CPSR)
    uint64_t _padding2; // Padding at offset 280
    __uint128_t q[32];  // Q0 - Q31 (Offset 288 - 799)
    uint32_t fpsr;      // Offset 800
    uint32_t fpcr;      // Offset 804
    uint64_t _padding3; // Padding for 16-byte stack alignment
} trapframe_t;

static void dump_stack() {
    uint64_t fp;
    
    asm volatile("mov %0, x29" : "=r"(fp));

    printf("\n--- Stack Trace ---\n");
    
    int depth = 0;
    while (fp != 0 && depth < 20) {
        if (fp < 0x40000000) break; 

        uint64_t lr = *(uint64_t*)(fp + 8);
        uint64_t prev_fp = *(uint64_t*)fp;

        printf("[%d] PC: 0x%lx\n", depth, lr - 0xba400000 + 0x40100000);

        fp = prev_fp;
        depth++;
    }
    printf("-------------------\n");
}

void el1_sync_handler(trapframe_t *frame) {
    u64 esr, elr, far;
    asm volatile("mrs %0, esr_el1" : "=r"(esr));
    asm volatile("mrs %0, elr_el1" : "=r"(elr));
    asm volatile("mrs %0, far_el1" : "=r"(far));

    u32 ec = (esr >> 26) & 0x3F;
    u64 iss = esr & 0x1FFFFFF;

    switch (ec) {
        case 0x20:  // Instruction Abort (Higher EL)
        case 0x21:  // Data Abort   (Higher EL)
        case 0x24:  // Instruction Abort (Lower EL)
        case 0x25:  // Data Abort   (Lower EL)
            u8 fsc = iss & 0x3F;
            
            // Extract Write flag (Bit 6 of ISS for Data Aborts)
            bool is_write = (iss >> 6) & 1; 

            printf("\n[PANIC] UNHANDLED SYNC EXCEPTION\n");
            printf("  Type: %s Abort\n", (ec & 0x1) ? "Data" : "Instruction");
            printf("  ESR: 0x%lx (FSC: 0x%x, Write: %d)\n", esr, fsc, is_write);
            printf("  FAR: 0x%lx\n", far);
            dump_stack();
            printf("[EXC] EL1 synchronous exception\n");
            break;

        case 0x3C:      // brk instruction
            return;

        default:
            printf("\n[PANIC] SYNC EXCEPTION\n");
            printf("  ESR: 0x%lx (EC: 0x%x)\n", esr, ec);
            printf("  ELR: 0x%lx (PC)\n", elr);
            printf("  FAR: 0x%lx (Addr)\n", far);
            dump_stack();
            printf("[EXC] EL1 synchronous exception\n");
            break;
    }
    
    while (1) asm volatile("wfe");
}

void el1_irq_handler() {

}

void el1_fiq_handler() {
    printf("[EXC] EL1 FIQ\n");
    dump_stack();
    while (1) asm volatile("wfe");
}

void el1_serr_handler() {
    dump_stack();
    printf("[EXC] EL1 SError\n");
    while (1) asm volatile("wfe");
}