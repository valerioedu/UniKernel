#include <stdint.h>

#define GIC_DIST_BASE   0x08000000
#define GIC_CPU_BASE    (GIC_DIST_BASE + 0x10000ULL)

#define GICD_CTLR       (GIC_DIST_BASE + 0x000)
#define GICD_ISENABLER  (GIC_DIST_BASE + 0x100)
#define GICD_IPRIORITY  (GIC_DIST_BASE + 0x400)
#define GICD_ITARGETS   (GIC_DIST_BASE + 0x800)

#define GICC_CTLR       (GIC_CPU_BASE + 0x0000)
#define GICC_PMR        (GIC_CPU_BASE + 0x0004)
#define GICC_IAR        (GIC_CPU_BASE + 0x000C)
#define GICC_EOIR       (GIC_CPU_BASE + 0x0010)

#define read32(address)      (*(volatile uint32_t*)(address))
#define write32(address, val) (*(volatile uint32_t*)(address) = (val))

void gic_init() {
    // Disables Distributor
    write32(GICD_CTLR, 0);

    // Enables CPU Interface
    write32(GICC_CTLR, 1);

    // Sets Priority Mask to allow all interrupts (0xFF)
    write32(GICC_PMR, 0xFF);

    // Enables Distributor
    write32(GICD_CTLR, 1);
}

void gic_enable_irq(uint64_t id) {
    // Enables register (1 bit per IRQ, 32 IRQs per register)
    uint64_t reg = GICD_ISENABLER + (id / 32) * 4;
    uint64_t bit = 1 << (id % 32);
    uint64_t val = read32(reg);
    write32(reg, val | bit);

    // 8 bits per IRQ, 4 IRQs per register
    if (id >= 32) {
        uint64_t target_reg = GICD_ITARGETS + (id / 4) * 4;
        uint64_t target_shift = (id % 4) * 8;
        uint64_t target_val = read32(target_reg);
        // Sets target to CPU 0 (bit 0 set -> 0x01)
        target_val |= (0x01 << target_shift);
        write32(target_reg, target_val);
    }
}

uint64_t gic_aoi() {
    return read32(GICC_IAR);
}

void gic_eoi(uint64_t id) {
    write32(GICC_EOIR, id);
}