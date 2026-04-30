#include <stdint.h>

#define PL031_DR        0x000   /* Data Register */
#define PL031_MR        0x004   /* Match Register */
#define PL031_LR        0x008   /* Load Register */
#define PL031_CR        0x00C   /* Control Register */
#define PL031_IMSC      0x010   /* Interrupt Mask Set/Clear */
#define PL031_RIS       0x014   /* Raw Interrupt Status */
#define PL031_MIS       0x018   /* Masked Interrupt Status */
#define PL031_ICR       0x01C   /* Interrupt Clear Register */

static volatile uint8_t *pl031 = (volatile uint8_t *)0x09010000;
static uint64_t boot_epoch = 0;
extern uint64_t boot_time;


static inline void mmio_write8(uintptr_t reg, uint8_t data) {
    *(volatile uint8_t*)reg = data;
}

static inline uint8_t mmio_read8(uintptr_t reg) {
    return *(volatile uint8_t*)reg;
}

static inline void mmio_write16(uintptr_t reg, uint16_t data) {
    *(volatile uint16_t*)reg = data;
}

static inline uint16_t mmio_read16(uintptr_t reg) {
    return *(volatile uint16_t*)reg;
}

static inline void mmio_write32(uintptr_t reg, uint32_t data) {
    *(volatile uint32_t*)reg = data;
}

static inline uint32_t mmio_read32(uintptr_t reg) {
    return *(volatile uint32_t*)reg;
}

static inline void mmio_write64(uintptr_t reg, uint64_t data) {
    *(volatile uint64_t*)reg = data;
}

static inline uint64_t mmio_read64(uintptr_t reg) {
    return *(volatile uint64_t*)reg;
}

void pl031_init_time() {
    if (pl031) boot_epoch = mmio_read32((uintptr_t)pl031 + PL031_DR);
}

uint32_t pl031_get_time() {
    if (!pl031) return 0;
    
    return mmio_read32((uintptr_t)pl031 + PL031_DR);
}

void pl031_set_time(uint32_t timestamp) {
    if (!pl031) return;

    mmio_write32((uintptr_t)pl031 + PL031_LR, timestamp);
}

void get_monotonic_time(uint64_t *sec, uint64_t *nsec) {
    uint64_t now, frq;
    asm volatile("mrs %0, cntpct_el0" : "=r"(now));
    asm volatile("mrs %0, cntfrq_el0" : "=r"(frq));

    uint64_t elapsed = now - boot_time;
    *sec  = elapsed / frq;
    *nsec = ((elapsed % frq) * 1000000000ULL) / frq;
}

void get_realtime(uint64_t *sec, uint64_t *nsec) {
    uint64_t mono_sec, mono_nsec;
    get_monotonic_time(&mono_sec, &mono_nsec);
    *sec  = boot_epoch + mono_sec;
    *nsec = mono_nsec;
}