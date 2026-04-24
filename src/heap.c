#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

#define PAGE_SIZE       4096
#define NUM_CACHE_SIZES (sizeof(slab_sizes) / sizeof(size_t))
#define PAGE_MASK       (~(PAGE_SIZE - 1))
#define HEAP_ALIGN      8
#define SLAB_MAGIC 0x51AB51AB

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

// Size classes for slab caches
static const size_t slab_sizes[] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};

typedef enum {
    PAGE_TYPE_FREE = 0,
    PAGE_TYPE_SLAB,
    PAGE_TYPE_LARGE,
} page_type_t;

typedef struct {
    u8 type;
    u16 count;
} page_info_t;

struct kmem_cache;

// Header stored at the beginning of a slab (4KB page)
typedef struct slab {
    struct slab *next;
    struct slab *prev;
    struct kmem_cache *cache; // Back pointer to the cache
    void *free_list;          // Head of the free objects list
    u32 in_use;               // Number of objects currently allocated
    u32 magic;
} slab_t;

typedef struct kmem_cache {
    slab_t *slabs_partial;    // Slabs with some free objects
    slab_t *slabs_full;       // Slabs that are completely full
    slab_t *slabs_free;       // Slabs that are completely empty (optional cache)
    size_t obj_size;          // Size of objects in this cache
} kmem_cache_t;

static pthread_spinlock_t heap_lock;
static uintptr_t heap_start_addr = 0;
static uintptr_t heap_end_addr = 0;
static page_info_t *page_metadata = NULL;
static u32 total_pages = 0;
static kmem_cache_t caches[NUM_CACHE_SIZES];

static inline size_t align_up(size_t n, size_t align) {
    return (n + align - 1) & ~(align - 1);
}

// Convert a virtual address to the page index in our heap
static inline u32 virt_to_page_idx(uintptr_t addr) {
    if (addr < heap_start_addr || addr >= heap_end_addr)
        return (u32)-1;
    
    return (addr - heap_start_addr) / PAGE_SIZE;
}

static inline uintptr_t page_idx_to_virt(u32 idx) {
    return heap_start_addr + (uintptr_t)idx * PAGE_SIZE;
}

// Allocates contiguous pages from the heap region
static void* alloc_pages_backend(size_t num_pages, page_type_t type) {
    u32 free_run = 0;
    u32 start_idx = (u32)-1;

    for (u32 i = 0; i < total_pages; i++) {
        if (page_metadata[i].type == PAGE_TYPE_FREE) {
            if (free_run == 0) start_idx = i;

            free_run++;
            if (free_run == num_pages) {
                for (u32 j = 0; j < num_pages; j++) {
                    page_metadata[start_idx + j].type = type;
                    page_metadata[start_idx + j].count = (j == 0) ? num_pages : 0;
                }

                return (void*)page_idx_to_virt(start_idx);
            }
        } else free_run = 0;
    }

    return NULL;
}

static void free_pages_backend(void* ptr) {
    u32 idx = virt_to_page_idx((uintptr_t)ptr);
    if (idx >= total_pages) return;

    u32 count = page_metadata[idx].count;
    if (count == 0) return;

    for (u32 i = 0; i < count; i++) {
        if (idx + i < total_pages) {
            page_metadata[idx + i].type = PAGE_TYPE_FREE;
            page_metadata[idx + i].count = 0;
        }
    }
}

static void slab_init_page(slab_t *slab, kmem_cache_t *cache) {
    slab->magic = SLAB_MAGIC;
    slab->cache = cache;
    slab->in_use = 0;
    slab->next = NULL;
    slab->prev = NULL;

    uintptr_t base = (uintptr_t)slab;
    uintptr_t start = base + sizeof(slab_t);
    start = align_up(start, HEAP_ALIGN);

    size_t available = PAGE_SIZE - (start - base);
    size_t max_objs = available / cache->obj_size;

    slab->free_list = (void*)start;
    uintptr_t curr = start;
    
    for (size_t i = 0; i < max_objs - 1; i++) {
        void **next_ptr = (void**)curr;
        *next_ptr = (void*)(curr + cache->obj_size);
        curr += cache->obj_size;
    }

    *((void**)curr) = NULL;
}

static void* cache_alloc(kmem_cache_t *c) {
    slab_t *slab = c->slabs_partial;

    if (!slab) {
        if (c->slabs_free) {
            slab = c->slabs_free;
            c->slabs_free = slab->next;
            if (c->slabs_free) c->slabs_free->prev = NULL;
        } else {
            slab = (slab_t*)alloc_pages_backend(1, PAGE_TYPE_SLAB);
            if (!slab) return NULL;
            slab_init_page(slab, c);
        }
        
        slab->next = c->slabs_partial;
        slab->prev = NULL;
        if (c->slabs_partial) c->slabs_partial->prev = slab;
        c->slabs_partial = slab;
    }

    void *obj = slab->free_list;
    slab->free_list = *((void**)obj);
    slab->in_use++;

    if (!slab->free_list) {
        if (slab->prev) 
            slab->prev->next = slab->next;
        
        else c->slabs_partial = slab->next;

        if (slab->next)
            slab->next->prev = slab->prev;

        slab->next = c->slabs_full;
        slab->prev = NULL;
        if (c->slabs_full)
            c->slabs_full->prev = slab;
        
        c->slabs_full = slab;
    }

    return obj;
}

static void cache_free(kmem_cache_t *c, void *ptr) {
    // Locate slab header from pointer (round down to page)
    slab_t *slab = (slab_t*)((uintptr_t)ptr & PAGE_MASK);
    
    if (slab->magic != SLAB_MAGIC) {
        printf("[HEAP] Corruption: Invalid slab magic at %p\n", slab);
        return;
    }

    bool was_full = (slab->free_list == NULL);

    // Add object back to free list
    *((void**)ptr) = slab->free_list;
    slab->free_list = ptr;
    slab->in_use--;

    if (slab->in_use == 0) {
        if (was_full) {
            if (slab->prev) 
                slab->prev->next = slab->next;

            else c->slabs_full = slab->next;

            if (slab->next) 
                slab->next->prev = slab->prev;
        } else {
            if (slab->prev)
                slab->prev->next = slab->next;
            
            else c->slabs_partial = slab->next;

            if (slab->next)
                slab->next->prev = slab->prev;
        }

        free_pages_backend(slab);
    } else if (was_full) {
        // Slab was full, now partial. Move from full to partial.
        if (slab->prev)
            slab->prev->next = slab->next;

        else c->slabs_full = slab->next;

        if (slab->next)
            slab->next->prev = slab->prev;
        
        slab->next = c->slabs_partial;
        slab->prev = NULL;
        if (c->slabs_partial) 
            c->slabs_partial->prev = slab;
        
        c->slabs_partial = slab;
    }
}

void heap_init(uintptr_t start, size_t size) {
    uintptr_t aligned_start = (start + PAGE_SIZE - 1) & PAGE_MASK;
    size_t diff = aligned_start - start;
    if (size < diff) return;
    size -= diff;
    
    total_pages = size / PAGE_SIZE;
    if (total_pages == 0) return;

    heap_start_addr = aligned_start;
    heap_end_addr = heap_start_addr + (total_pages * PAGE_SIZE);

    // Place metadata at the start of the heap
    size_t meta_size = total_pages * sizeof(page_info_t);
    size_t meta_pages = (meta_size + PAGE_SIZE - 1) / PAGE_SIZE;

    if (meta_pages >= total_pages) {
        printf("[HEAP] Error: Heap too small for metadata\n");
        return;
    }

    page_metadata = (page_info_t*)heap_start_addr;
    memset(page_metadata, 0, meta_size);

    // Reserve metadata pages
    for (size_t i = 0; i < meta_pages; i++) {
        page_metadata[i].type = PAGE_TYPE_LARGE; // Reserved
        page_metadata[i].count = 1;
    }

    // Initialize Caches
    for (int i = 0; i < NUM_CACHE_SIZES; i++) {
        caches[i].obj_size = slab_sizes[i];
        caches[i].slabs_full = NULL;
        caches[i].slabs_partial = NULL;
        caches[i].slabs_free = NULL;
    }

    pthread_spin_init(&heap_lock, 0);
    printf("[HEAP] Slab Allocator Initialized: %d MB (%d pages)\n", size/1024/1024, total_pages);
}

void* malloc(size_t size) {
    if (size == 0) return NULL;

    pthread_spin_lock(&heap_lock);

    void *ptr = NULL;

    if (size <= 2048) {
        for (int i = 0; i < NUM_CACHE_SIZES; i++) {
            if (size <= caches[i].obj_size) {
                ptr = cache_alloc(&caches[i]);
                break;
            }
        }
    } else {
        size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        ptr = alloc_pages_backend(pages_needed, PAGE_TYPE_LARGE);
    }

    pthread_spin_unlock(&heap_lock);
    return ptr;
}

void free(void* ptr) {
    if (!ptr) return;

    pthread_spin_lock(&heap_lock);

    u32 idx = virt_to_page_idx((uintptr_t)ptr);
    if (idx < total_pages) {
        if (page_metadata[idx].type == PAGE_TYPE_SLAB) {
            slab_t *slab = (slab_t*)((uintptr_t)ptr & PAGE_MASK);
            if (slab->cache) {
                cache_free(slab->cache, ptr);
            }
        } else if (page_metadata[idx].type == PAGE_TYPE_LARGE) {
            free_pages_backend(ptr);
        } else {
            printf("[HEAP] Warning: Double free or invalid free at %p\n", ptr);
        }
    }

    pthread_spin_unlock(&heap_lock);
}

void* calloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void* realloc(void* ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    // Determine old size
    size_t old_size = 0;
    
    pthread_spin_lock(&heap_lock);
    u32 idx = virt_to_page_idx((uintptr_t)ptr);
    
    if (idx < total_pages) {
        if (page_metadata[idx].type == PAGE_TYPE_SLAB) {
            slab_t *slab = (slab_t*)((uintptr_t)ptr & PAGE_MASK);
            if (slab->cache) old_size = slab->cache->obj_size;
        } else if (page_metadata[idx].type == PAGE_TYPE_LARGE) {
            old_size = page_metadata[idx].count * PAGE_SIZE;
        }
    }

    pthread_spin_unlock(&heap_lock);

    if (old_size >= new_size) return ptr;

    void* new_ptr = malloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_size);
        free(ptr);
    }

    return new_ptr;
}

void heap_debug() {
    pthread_spin_lock(&heap_lock);

    printf("--- Slab Allocator Status ---\n");
    for (int i = 0; i < NUM_CACHE_SIZES; i++) {
        kmem_cache_t *c = &caches[i];
        if (c->slabs_partial || c->slabs_full) {
            printf("Cache %d: Full: %s, Partial: %s\n", 
                c->obj_size, 
                c->slabs_full ? "Yes" : "No", 
                c->slabs_partial ? "Yes" : "No");
        }
    }

    pthread_spin_unlock(&heap_lock);
}