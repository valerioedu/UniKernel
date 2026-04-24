#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define PT_VALID    (1ULL << 0)
#define PT_TABLE    (1ULL << 1)
#define PT_PAGE     (1ULL << 1)
#define PT_BLOCK    (0ULL << 1)

#define PT_AF       (1ULL << 10) // Access Flag
#define PT_SH_INNER (3ULL << 8)  // Inner Shareable
#define PT_SW_COW   (1ULL << 55)

#define MT_NORMAL        1
#define PT_AP_RW_EL1    (0ULL << 6)
#define PT_AP_RW_EL0    (1ULL << 6)
#define PT_AP_RO_EL1    (2ULL << 6)
#define PT_AP_RO_EL0    (3ULL << 6)
#define PT_PXN          (1ULL << 53)
#define PT_UXN          (1ULL << 54)

#define VM_WRITABLE  (1ULL << 0)
#define VM_USER      (1ULL << 1)
#define VM_NO_EXEC   (1ULL << 2)
#define VM_DEVICE    (1ULL << 3) // Memory Mapped I/O (nGnRnE)

#define BLOCK_SIZE  (1ULL << 21)

void init_vmm();
void vmm_map_page(uintptr_t virt, uintptr_t phys, uint64_t flags);
void vmm_map_region(uintptr_t virt, uintptr_t phys, size_t size, uint64_t flags);
void dcache_clean_poc(void *addr, size_t size);

uint64_t* vmm_get_pte_from_table(uint64_t* page_table, uintptr_t virt);
uint64_t* vmm_get_pte_from_table_alloc(uint64_t* page_table, uintptr_t virt);

#endif