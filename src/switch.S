.global cpu_switch_to
.global ret_from_fork
.extern sched_unlock_release
.extern pthread_exit

# void cpu_switch_to(struct task* prev, struct task* next);
# x0 = pointer to 'prev' task struct
# x1 = pointer to 'next' task struct
cpu_switch_to:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]

    mov x9, sp
    str x9, [x0, #96]

    stp q8,  q9,  [x0, #112]
    stp q10, q11, [x0, #144]
    stp q12, q13, [x0, #176]
    stp q14, q15, [x0, #208]

    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]

    ldr x9, [x1, #96]
    mov sp, x9

    ldp q8,  q9,  [x1, #112]
    ldp q10, q11, [x1, #144]
    ldp q12, q13, [x1, #176]
    ldp q14, q15, [x1, #208]

    ret

ret_from_fork:
    bl sched_unlock_release

#   Calls the entry point function
    mov x0, x20
    blr x19
    bl pthread_exit
    b .
