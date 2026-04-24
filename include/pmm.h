#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PHY_RAM_BASE    0x40000000ULL

extern uint64_t phy_ram_size;
extern uint64_t phy_ram_end;
extern uint32_t total_frames;

#define PAGE_SIZE       4096ULL
#define PAGE_SHIFT      12

void pmm_init(uintptr_t kernel_end, uint64_t ram_size);
uintptr_t pmm_alloc_frame();
void pmm_free_frame(uintptr_t addr);
void pmm_mark_used_region(uintptr_t base, size_t size);
void pmm_inc_ref(uintptr_t phys);

#endif