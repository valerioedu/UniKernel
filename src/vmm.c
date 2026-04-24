#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "pmm.h"
#include "vmm.h"

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define MT_DEVICE_nGnRnE 0
#define MT_NORMAL        1

#define PT_AP_RW_EL1    (0ULL << 6)     // Read/Write, Kernel Only
#define PT_AP_RO_EL1    (2ULL << 6)     // Read-Only,  Kernel Only

#define PT_PXN          (1ULL << 53)    // Privileged Execute Never

extern u64 _kernel_end;

u64* root_table;
u64 root_phys;

static bool paging_enabled = false;

static inline void isb() { asm volatile("isb" ::: "memory"); }
static inline void dsb() { asm volatile("dsb ish" ::: "memory"); }

static inline void tlb_flush() {
    asm volatile("tlbi vmalle1is"); 
    dsb();
    isb();
}

static inline void write_mair(u64 val) { asm volatile("msr mair_el1, %0" :: "r"(val) : "memory"); }
static inline void write_tcr(u64 val)  { asm volatile("msr tcr_el1, %0" :: "r"(val) : "memory"); }
static inline void write_ttbr0(u64 val){ asm volatile("msr ttbr0_el1, %0" :: "r"(val) : "memory"); }
static inline void write_ttbr1(u64 val){ asm volatile("msr ttbr1_el1, %0" :: "r"(val) : "memory"); }
static inline void write_sctlr(u64 val){ asm volatile("msr sctlr_el1, %0" :: "r"(val) : "memory"); }

static u64* get_next_table(u64* table, u64 idx) {
    if (table[idx] & PT_VALID) {
        // For 4KB granule, output address is bits [47:12]
        u64 phys = table[idx] & 0x0000FFFFFFFFF000ULL;
        return (u64*)phys;
    }

    // Invalid
    u64 new_table_phys = pmm_alloc_frame();
    if (!new_table_phys) return NULL;   // Out of memory
    
    u64* new_table_virt = (u64*)new_table_phys;

    memset((void*)new_table_virt, 0, PAGE_SIZE);
    
    table[idx] = new_table_phys | PT_TABLE | PT_VALID;

    return (u64*)new_table_virt;
}

void dcache_clean_poc(void *addr, size_t size) {
    uintptr_t start = (uintptr_t)addr & ~(64 - 1);
    uintptr_t end = (uintptr_t)addr + size;
    
    asm volatile("dsb sy" ::: "memory");
    while (start < end) {
        asm volatile("dc cvac, %0" :: "r"(start) : "memory");
        start += 64;
    }
    asm volatile("dsb sy" ::: "memory");
    asm volatile("isb" ::: "memory");
}

// Maps a single 4KB page in kernel space
void vmm_map_page(uintptr_t virt, uintptr_t phys, u64 flags) {
    u64 l1_idx = (virt >> 30) & 0x1FF;
    u64 l2_idx = (virt >> 21) & 0x1FF;
    u64 l3_idx = (virt >> 12) & 0x1FF;

    // Walk L1 -> L2
    u64* l2_table = get_next_table(root_table, l1_idx);
    if (!l2_table) {
        printf("[ [CVMM [W] Failed to allocate L2 table for 0x%llx\n", virt);
        return;
    }

    // Walk L2 -> L3
    u64 *l3_table = get_next_table(l2_table, l2_idx);
    if (!l3_table) {
        printf("[ [CVMM [W] Failed to allocate L3 table for 0x%llx\n", virt);
        return;
    }

    u64 entry = phys | PT_PAGE | PT_VALID | PT_AF | PT_SH_INNER;
    
    if (flags & VM_DEVICE) {
        entry |= PT_PXN | PT_UXN; // Prevent execution from MMIO fully
    } else {
        entry |= (MT_NORMAL << 2);
    }

    // Unikernel: Everything is mapped for Kernel (EL1) access only.
    // Unprivileged Execute Never is set as a good safety measure against EL0 drops.
    entry |= PT_UXN;
    
    if (flags & VM_WRITABLE) {
        entry |= PT_AP_RW_EL1;
    } else {
        entry |= PT_AP_RO_EL1;
    }

    // Apply executable restrictions if requested
    if (flags & VM_NO_EXEC) {
        entry |= PT_PXN;
    }

    l3_table[l3_idx] = entry;
}

void vmm_map_block(uintptr_t virt, uintptr_t phys, u64 flags) {
    u64 l1_idx = (virt >> 30) & 0x1FF;
    u64 l2_idx = (virt >> 21) & 0x1FF;

    // Walk L1 -> L2
    u64* l2_table = get_next_table(root_table, l1_idx);
    if (!l2_table) {
        printf("[ [CVMM [W] Failed to allocate L2 table for 0x%llx\n", virt);
        return;
    }

    u64 entry = phys | PT_BLOCK | PT_VALID | PT_AF | PT_SH_INNER;
    
    if (flags & VM_DEVICE) {
        entry |= PT_PXN | PT_UXN; // Prevent execution from MMIO fully
    } else {
        entry |= (MT_NORMAL << 2);
    }

    // Unikernel: Everything is mapped for Kernel (EL1) access only.
    // Unprivileged Execute Never is set as a good safety measure against EL0 drops.
    entry |= PT_UXN;
    
    if (flags & VM_WRITABLE) {
        entry |= PT_AP_RW_EL1;
    } else {
        entry |= PT_AP_RO_EL1;
    }

    // Apply executable restrictions if requested
    if (flags & VM_NO_EXEC) {
        entry |= PT_PXN;
    }

    l2_table[l2_idx] = entry;
}

// Maps a contiguous range of physical memory
void vmm_map_region(uintptr_t virt, uintptr_t phys, size_t size, u64 flags) {
    size_t mapped = 0;
    while (mapped < size) {
        // If everything is 2MB aligned and we have at least 2MB left to map
        if ((virt + mapped) % BLOCK_SIZE == 0 && 
            (phys + mapped) % BLOCK_SIZE == 0 && 
            (size - mapped) >= BLOCK_SIZE) {
            
            vmm_map_block(virt + mapped, phys + mapped, flags);
            mapped += BLOCK_SIZE;
        } else {
            // Fallback to strict 4KB pages
            vmm_map_page(virt + mapped, phys + mapped, flags);
            mapped += PAGE_SIZE;
        }
    }
}

void init_vmm() {
    root_phys = pmm_alloc_frame();
    memset((void*)root_phys, 0, PAGE_SIZE);

    root_table = (u64*)root_phys;

    // Index 0: Device-nGnRnE (Strict order, no cache)
    // Index 1: Normal Memory (Outer/Inner Write-Back Cacheable)
    u64 mair = (0x00UL << (8 * MT_DEVICE_nGnRnE)) | 
                    (0xFFUL << (8 * MT_NORMAL));
    write_mair(mair);

    // Setup Translation Control (TCR)
    u64 tcr = (25UL << 0) | (0UL << 14) | (2UL << 32) | (25UL << 16) | (2UL << 30);
    write_tcr(tcr);

    // Kernel map (Identity)
    u64 kernel_size = (u64)&_kernel_end - 0x40000000;
    vmm_map_region(0x40000000, 0x40000000, kernel_size, VM_WRITABLE);

    vmm_map_region(PHY_RAM_BASE, PHY_RAM_BASE, phy_ram_size, VM_WRITABLE);

    // Heap map (Identity)
    vmm_map_region(0x50000000, 0x50000000, 8 * 1024 * 1024, VM_WRITABLE);

    // UART map (Identity)
    vmm_map_region(0x09000000, 0x09000000, 1024 * 1024, VM_WRITABLE | VM_DEVICE);

    // Activates Paging
    write_ttbr0(root_phys);
    write_ttbr1(root_phys); 

    tlb_flush();

    // Enables MMU and Caches
    u64 sctlr;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1 << 0) | (1 << 2) | (1 << 12);
    write_sctlr(sctlr);

    isb();

    paging_enabled = true;
    
    printf("[VMM] Identity Mapping Initialized.\n");
}