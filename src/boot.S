.section .text.boot
.global _start

_start:
#   Linux boot protocol header
    b entry_point
    .long 0
    .quad 0x100000
    .quad _kernel_end - _start
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .ascii "ARM\x64"
    .long 0

entry_point:
    ldr x1, =dtb_phys_addr
    str x0, [x1]

#   Stack setup
    ldr x0, =_stack_top
    mov sp, x0

#   EL1 vector base
    adr x0, el1_vector_table
    msr VBAR_EL1, x0
    isb

#   Enables FP/SIMD
    mrs x0, CPACR_EL1
    orr x0, x0, #(3 << 20)
    msr CPACR_EL1, x0
    isb

#   Sets MMU off
    mrs x0, SCTLR_EL1
    bic x0, x0, #(1 << 0)
    msr SCTLR_EL1, x0
    isb

#   Unmask interrupts (DAIF) to allow IRQs to be handled
    msr daifclr, #2

#   Clears BSS
    ldr x1, =__bss_start
    ldr x2, =__bss_end
    sub x2, x2, x1
    cbz x2, 2f
    mov x3, #0
1:
    str x3, [x1], #8
    sub x2, x2, #8
    cbnz x2, 1b
2:
    bl main
hang:
    wfe
    b hang

.macro SAVE_CONTEXT
    // Allocate 816 bytes for the new trapframe_t
    sub sp, sp, #816
    
    stp x0, x1, [sp, #16 * 0]
    stp x2, x3, [sp, #16 * 1]
    stp x4, x5, [sp, #16 * 2]
    stp x6, x7, [sp, #16 * 3]
    stp x8, x9, [sp, #16 * 4]
    stp x10, x11, [sp, #16 * 5]
    stp x12, x13, [sp, #16 * 6]
    stp x14, x15, [sp, #16 * 7]
    stp x16, x17, [sp, #16 * 8]
    stp x18, x19, [sp, #16 * 9]
    stp x20, x21, [sp, #16 * 10]
    stp x22, x23, [sp, #16 * 11]
    stp x24, x25, [sp, #16 * 12]
    stp x26, x27, [sp, #16 * 13]
    stp x28, x29, [sp, #16 * 14]
    str x30, [sp, #16 * 15] 
    
    mrs x0, sp_el0
    str x0, [sp, #256]

    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, #264] // elr and spsr
    
    // Save all 32 Vector/SIMD registers
    stp q0, q1, [sp, #288]  // Fixed from v0 to q0
    stp q2, q3, [sp, #320]
    stp q4, q5, [sp, #352]
    stp q6, q7, [sp, #384]
    stp q8, q9, [sp, #416]
    stp q10, q11, [sp, #448]
    stp q12, q13, [sp, #480]
    stp q14, q15, [sp, #512]
    stp q16, q17, [sp, #544]
    stp q18, q19, [sp, #576]
    stp q20, q21, [sp, #608]
    stp q22, q23, [sp, #640]
    stp q24, q25, [sp, #672]
    stp q26, q27, [sp, #704]
    stp q28, q29, [sp, #736]
    stp q30, q31, [sp, #768]

    // Save Floating-Point Status and Control Registers
    mrs x21, fpsr
    mrs x22, fpcr
    // Swapped stp for two str instructions to bypass offset limits
    str w21, [sp, #800] 
    str w22, [sp, #804] 
.endm

.macro RESTORE_CONTEXT
    // Restore Floating-Point Status and Control Registers
    // Swapped ldp for two ldr instructions
    ldr w21, [sp, #800]
    ldr w22, [sp, #804]
    msr fpsr, x21
    msr fpcr, x22

    // Restore all 32 Vector/SIMD registers
    ldp q0, q1, [sp, #288]
    ldp q2, q3, [sp, #320]
    ldp q4, q5, [sp, #352]
    ldp q6, q7, [sp, #384]
    ldp q8, q9, [sp, #416]
    ldp q10, q11, [sp, #448]
    ldp q12, q13, [sp, #480]
    ldp q14, q15, [sp, #512]
    ldp q16, q17, [sp, #544]
    ldp q18, q19, [sp, #576]
    ldp q20, q21, [sp, #608]
    ldp q22, q23, [sp, #640]
    ldp q24, q25, [sp, #672]
    ldp q26, q27, [sp, #704]
    ldp q28, q29, [sp, #736]
    ldp q30, q31, [sp, #768]

    ldp x0, x1, [sp, #264]
    msr elr_el1, x0
    msr spsr_el1, x1

    ldr x0, [sp, #256]
    msr sp_el0, x0

    ldp x0, x1, [sp, #16 * 0]
    ldp x2, x3, [sp, #16 * 1]
    ldp x4, x5, [sp, #16 * 2]
    ldp x6, x7, [sp, #16 * 3]
    ldp x8, x9, [sp, #16 * 4]
    ldp x10, x11, [sp, #16 * 5]
    ldp x12, x13, [sp, #16 * 6]
    ldp x14, x15, [sp, #16 * 7]
    ldp x16, x17, [sp, #16 * 8]
    ldp x18, x19, [sp, #16 * 9]
    ldp x20, x21, [sp, #16 * 10]
    ldp x22, x23, [sp, #16 * 11]
    ldp x24, x25, [sp, #16 * 12]
    ldp x26, x27, [sp, #16 * 13]
    ldp x28, x29, [sp, #16 * 14]
    ldr x30, [sp, #16 * 15]
    
    // Free 816 bytes from stack
    add sp, sp, #816
    eret
.endm

.section .data
.align 3
.global dtb_phys_addr
dtb_phys_addr:
    .quad 0

.section .text

.extern el1_sync_handler
.extern el1_irq_handler
.extern el1_fiq_handler
.extern el1_serr_handler

.global irq_entry
irq_entry:
    SAVE_CONTEXT
    mov x0, sp
    bl el1_irq_handler
    RESTORE_CONTEXT

.global sync_entry
sync_entry:
    SAVE_CONTEXT
    mov x0, sp
    bl el1_sync_handler
    RESTORE_CONTEXT
    
.align 11
.global el1_vector_table
el1_vector_table:
    .align 7
    b sync_entry
    .align 7
    b irq_entry
    .align 7
    b el1_fiq_handler
    .align 7
    b el1_serr_handler
    
    .align 7
    b sync_entry
    .align 7
    b irq_entry
    .align 7
    b el1_fiq_handler
    .align 7
    b el1_serr_handler
    
    .align 7
    b sync_entry
    .align 7
    b irq_entry
    .align 7
    b el1_fiq_handler
    .align 7
    b el1_serr_handler
    
    .align 7
    b sync_entry
    .align 7
    b irq_entry
    .align 7
    b el1_fiq_handler
    .align 7
    b el1_serr_handler

.section .data
.global vbar_debug
vbar_debug:
    .quad 0

.global secondary_entry
secondary_entry:
    ldr x1, [x0, #0]
    mov sp, x1
    
    mrs x1, CPACR_EL1
    orr x1, x1, #(3 << 20)
    msr CPACR_EL1, x1
    isb

    ldr x1, [x0, #8]
    msr ttbr0_el1, x1
    msr ttbr1_el1, x1

    ldr x1, [x0, #24]
    msr mair_el1, x1
    
    ldr x1, [x0, #16]
    msr tcr_el1, x1
    isb

    mrs x1, sctlr_el1
    orr x1, x1, #1 
    orr x1, x1, #(1 << 2)
    orr x1, x1, #(1 << 12)
    msr sctlr_el1, x1
    isb

    ldr x1, [x0, #32]
    br x1
