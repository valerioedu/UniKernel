#include <sched.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#ifndef MAX_CPUS
#define MAX_CPUS 2
#endif

#define current_task (get_core()->task)

//TODO: be POSIX pthread.h compatible

struct cpu_context {
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t fp;  // x29
    uint64_t lr;  // x30
    uint64_t sp;

    __uint128_t q8;
    __uint128_t q9;
    __uint128_t q10;
    __uint128_t q11;
    __uint128_t q12;
    __uint128_t q13;
    __uint128_t q14;
    __uint128_t q15;
};

typedef enum task_priority {
    IDLE = 0,
    LOW,
    NORMAL,
    HIGH,
    REALTIME,

    /* Automatically 5 */
    COUNT
} task_priority;

typedef enum task_state {
    TASK_RUNNING = 0,
    TASK_READY = 1,
    TASK_EXITED = 2,
    TASK_BLOCKED = 3,
    TASK_STOPPED = 4,
    TASK_SLEEPING = 5
} task_state;

typedef struct task {
    struct cpu_context context; // Must be at offset 0 for easier assembly
    uint64_t id;
    uint64_t wake_tick;
    task_state state;
    task_priority priority;
    struct task* next;          // Linked list pointer
    struct task *prev;
    struct task* next_wait;     // Pointer to wait queue
    struct task *sleep_next;
    void* stack_page;           // Pointer to the allocated stack memory
    void *ret_value;
    struct task *join_wait_queue;
} task_t;

typedef struct {
    uint32_t cpu_id;
    task_t *task;
} cpu_core_t;

static inline cpu_core_t* get_core() {
    cpu_core_t* core;
    asm volatile("mrs %0, tpidr_el1" : "=r"(core));
    return core;
}

extern void cpu_switch_to(struct task* prev, struct task* next);
extern void ret_from_fork();
int sched_yield();
int pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr, void *(*entry_point)(void*), void *restrict arg);

static task_t *runqueues[COUNT];
static task_t *runqueues_tail[COUNT];
static task_t *sleep_queue = NULL;
static cpu_core_t cores[MAX_CPUS];

static uint64_t tid_counter = 0;

static pthread_spinlock_t sched_lock;
static uint32_t active_priorities = 0;
static uint64_t kernel_ttbr1 = 0;

static uint64_t idle_flag;

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

void sched_unlock_release() {
    pthread_spin_unlock(&sched_lock);
}

//TODO: Implement atomic operations
static void sched_enqueue_task(task_t *t) {
    task_priority prio = t->priority;
    t->next = NULL;
    t->prev = NULL;
    
    if (runqueues[prio] == NULL) {
        runqueues[prio] = t;
        runqueues_tail[prio] = t;
    } else {
        t->prev = runqueues_tail[prio];
        runqueues_tail[prio]->next = t;
        runqueues_tail[prio] = t;
    }
    
    active_priorities |= (1 << prio);
}

//TODO: Implement atomic operations
static void sched_dequeue_task(task_t *t) {
    task_priority prio = t->priority;
    
    if (t->prev) {
        t->prev->next = t->next;
    } else {
        if (runqueues[prio] == t)
            runqueues[prio] = t->next; 
    }

    if (t->next) {
        t->next->prev = t->prev;
    } else {
        if (runqueues_tail[prio] == t)
            runqueues_tail[prio] = t->prev; 
    }

    t->next = NULL;
    t->prev = NULL;

    if (runqueues[prio] == NULL) {
        active_priorities &= ~(1 << prio);
    }
}

static void *idle(void *arg) {
    while (true) {
        asm volatile("yield");
        sched_yield();
    }
}

void sched_init() {
    for (int i = 0; i < 2; i++) {
        cores[i].cpu_id = i;
        cores[i].task = NULL;
    }

    cpu_core_t* primary_core = &cores[0];
    asm volatile("msr tpidr_el1, %0" :: "r"(primary_core));
    
    // Clears all queues
    for (int i = 0; i < COUNT; i++) {
        runqueues[i] = NULL;
        runqueues_tail[i] = NULL;
    }

    // Store the kernel's TTBR1 value
    asm volatile("mrs %0, ttbr1_el1" : "=r"(kernel_ttbr1));
    pthread_create(NULL, NULL, idle, &idle_flag);
    
    task_t* main_task = (task_t*)malloc(sizeof(task_t));
    memset(main_task, 0, sizeof(task_t));
    
    // We don't allocate a stack for main_task because it is already 
    // safely running on the boot stack (_stack_top).
    main_task->id = tid_counter++;
    main_task->state = TASK_RUNNING; 
    main_task->priority = NORMAL;
    
    current_task = main_task;
    sched_enqueue_task(main_task);

    printf("[SCHED] Multi-Queue Scheduler Initialized (%d Levels).\n", COUNT);
}

int pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr, void *(*start_routine)(void*), void *restrict arg) {
    task_t* t = (task_t*)malloc(sizeof(task_t));
    if (!t) {
        return EAGAIN;
    }

    memset(t, 0, sizeof(task_t));
    
    if (attr && attr->is_initialized && !attr->stackaddr) {
        t->stack_page = malloc(attr->stacksize);
    } else if (attr && attr->is_initialized && attr->stackaddr) {
        t->stack_page = attr->stackaddr;
    } else {
        t->stack_page = malloc(4096 * 4);
    }

    if (!t->stack_page) {
        printf("[SCHED] Failed to allocate stack!\n");
        free(t);
        return EAGAIN;
    }

    t->id = tid_counter++;
    t->state = TASK_READY;
    if (arg == &idle_flag) {
        t->priority = IDLE;
    } else if (attr && attr->is_initialized) {
        t->priority = attr->schedparam % 5;
    } else {
        t->priority = NORMAL;
    }
    
    t->next = NULL;
    t->prev = NULL;
    t->next_wait = NULL;

    uint64_t stack_size = 4096 * 4;       
    if (attr && attr->is_initialized) {
        stack_size = attr->stacksize;
    }

    if (attr && attr->is_initialized && attr->stackaddr) {
        t->stack_page = attr->stackaddr;
    } else {
        t->stack_page = malloc(stack_size);
    }

    if (!t->stack_page) {
        printf("[RSCHED] Failed to allocate stack!\n");
        free(t);
        return EAGAIN;
    }

    t->context.sp = (uint64_t)t->stack_page + stack_size;
    t->context.lr  = (uint64_t)ret_from_fork;
    t->context.x20 = (uint64_t)arg;
    t->context.x19 = (uint64_t)start_routine;

    pthread_spin_lock(&sched_lock);
    sched_enqueue_task(t);

    pthread_spin_unlock(&sched_lock);

    if (thread) {
        *thread = (void*)t;
    }

    return 0;
}

//TODO: Implement atomics
[[noreturn]] void pthread_exit(void *value_ptr) {
    pthread_spin_lock(&sched_lock);
    
    current_task->ret_value = value_ptr;
    current_task->state = TASK_EXITED;
    
    task_t* waiting = current_task->join_wait_queue;
    while (waiting) {
        task_t* next = waiting->next_wait;
        
        waiting->state = TASK_READY;
        sched_enqueue_task(waiting);
        
        waiting->next_wait = NULL;
        waiting = next;
    }

    current_task->join_wait_queue = NULL;
    task_t *t = current_task;

    sched_dequeue_task(current_task);
    pthread_spin_unlock(&sched_lock);
    
    free(t);
    free(t->stack_page);
    sched_yield();
    while(1); 
}

static void rotate_queue(task_priority priority) {
    task_t *head = runqueues[priority];
    if (!head || !head->next) return;

    sched_dequeue_task(head);
    sched_enqueue_task(head);
}

int sched_yield() {
    pthread_spin_lock(&sched_lock);

    task_t* prev_task = current_task;
    task_t* next_task = NULL;

    if (prev_task && prev_task->state == TASK_RUNNING)
        rotate_queue(prev_task->priority);


    if (active_priorities == 0)
        next_task = runqueues[IDLE];
    
    else {
        int highest_prio = 31 - __builtin_clz(active_priorities);
        if (highest_prio >= COUNT) highest_prio = COUNT - 1;
        next_task = runqueues[highest_prio];
    }

    if (!next_task) {
        next_task = runqueues[IDLE];
    }

    if (next_task != prev_task) {
        current_task = next_task;

        if (current_task->state == TASK_READY) {
            current_task->state = TASK_RUNNING;
        }

        cpu_switch_to(prev_task, next_task);
    }

    pthread_spin_unlock(&sched_lock);
    return 0;
}

int pthread_attr_init(pthread_attr_t *attr) {
    if (!attr) {
        return EINVAL;
    }

    attr->detachstate = 0;
    attr->stacksize = 4096 * 4;
    attr->stackaddr = NULL;
    attr->schedpolicy = 0;
    attr->schedparam = 2;
    attr->is_initialized = 1;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr) {
    if (!attr) {
        return EINVAL;
    }

    attr->is_initialized = 0;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {
    if (!attr || !attr->is_initialized) return EINVAL;
    
    if (stacksize < 4096) return EINVAL;
    
    attr->stacksize = stacksize;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *restrict attr, size_t *restrict stacksize) {
    if (!attr || !attr->is_initialized || !stacksize) return EINVAL;
    *stacksize = attr->stacksize;
    return 0;
}

int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize) {
    if (!attr || !attr->is_initialized) return EINVAL;
    if (stacksize < 4096) return EINVAL;
    
    attr->stackaddr = stackaddr;
    attr->stacksize = stacksize;
    return 0;
}

int pthread_attr_getstack(const pthread_attr_t *restrict attr, void **restrict stackaddr, size_t *restrict stacksize) {
    if (!attr || !attr->is_initialized || !stackaddr || !stacksize) return EINVAL;
    
    *stackaddr = attr->stackaddr;
    *stacksize = attr->stacksize;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
    if (!attr || !attr->is_initialized) return EINVAL;
    
    if (detachstate != 0 && detachstate != 1) return EINVAL;
    
    attr->detachstate = detachstate;
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate) {
    if (!attr || !attr->is_initialized || !detachstate) return EINVAL;
    *detachstate = attr->detachstate;
    return 0;
}

int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy) {
    if (!attr || !attr->is_initialized) return EINVAL;

    attr->schedpolicy = policy;
    return 0;
}

int pthread_attr_getschedpolicy(const pthread_attr_t *restrict attr, int *restrict policy) {
    if (!attr || !attr->is_initialized || !policy) return EINVAL;
    *policy = attr->schedpolicy;
    return 0;
}

int pthread_attr_setschedparam(pthread_attr_t *restrict attr, const int *restrict param) {
    if (!attr || !attr->is_initialized || !param) return EINVAL;
    
    if (*param < 0 || *param >= COUNT) return EINVAL;
    
    attr->schedparam = *param;
    return 0;
}

int pthread_attr_getschedparam(const pthread_attr_t *restrict attr, int *restrict param) {
    if (!attr || !attr->is_initialized || !param) return EINVAL;
    *param = attr->schedparam;
    return 0;
}

void sched_init_secondary(int cpu_id) {
    if (cpu_id < 0 || cpu_id >= MAX_CPUS) return;

    cores[cpu_id].cpu_id = cpu_id;
    cores[cpu_id].task = NULL;

    cpu_core_t* local_core = &cores[cpu_id];
    asm volatile("msr tpidr_el1, %0" :: "r"(local_core));

    task_t* main_task = (task_t*)malloc(sizeof(task_t));
    memset(main_task, 0, sizeof(task_t));
    
    main_task->id = tid_counter++;
    main_task->state = TASK_RUNNING;
    main_task->priority = IDLE;
    
    current_task = main_task;
}
}