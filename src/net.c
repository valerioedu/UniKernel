#include <stddef.h>
#include <stdio.h>
#include "net.h"
#include "pmm.h"

#define STATUS_ACKNOWLEDGE  1
#define STATUS_DRIVER       2
#define STATUS_FEATURES_OK  8
#define STATUS_DRIVER_OK    4
#define STATUS_FAILED       128
#define VIRTIO_REG_CONFIG   0x100

#define read32(offset)      (*(volatile uint32_t*)(VIRTIO_NET_BASE + (offset)))
#define write32(offset, val) (*(volatile uint32_t*)(VIRTIO_NET_BASE + (offset)) = (val))

typedef struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) virtq_desc;

typedef struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];    // Length = qsize
} __attribute__((packed)) virtq_avail;

typedef struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) virtq_used_elem;

typedef struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[]; // Length = qsize
} __attribute__((packed)) virtq_used;

typedef struct virtq {
    virtq_desc *desc;
    virtq_avail *avail;
    virtq_used *used;
    uint16_t last_used_idx;
} virtq;

typedef struct virtio_net_config {
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
} __attribute__((packed)) virtio_net_config;

static virtq rxq;
static virtq txq;
static uint8_t net_mac[6];

static void virtio_init_queue(uint32_t queue_index, virtq* queue) {
    write32(VIRTIO_REG_QUEUE_SEL, queue_index);
    
    uint32_t qsize = read32(VIRTIO_REG_QUEUE_MAX);
    if (qsize == 0) {
        printf("Queue %d is unavailable!\n", queue_index);
        return;
    }

    uintptr_t desc_phys = pmm_alloc_frame();
    uintptr_t avail_phys = pmm_alloc_frame(); 
    uintptr_t used_phys = pmm_alloc_frame();

    queue->desc = (virtq_desc*)desc_phys;
    queue->avail = (virtq_avail*)avail_phys;
    queue->used = (virtq_used*)used_phys;
    queue->last_used_idx = 0;

    write32(VIRTIO_REG_Q_DESC_LOW, (uint32_t)desc_phys);
    write32(VIRTIO_REG_Q_DESC_HIGH, (uint32_t)(desc_phys >> 32));

    write32(VIRTIO_REG_Q_AVAIL_LOW, (uint32_t)avail_phys);
    write32(VIRTIO_REG_Q_AVAIL_HIGH, (uint32_t)(avail_phys >> 32));

    write32(VIRTIO_REG_Q_USED_LOW, (uint32_t)used_phys);
    write32(VIRTIO_REG_Q_USED_HIGH, (uint32_t)(used_phys >> 32));

    write32(VIRTIO_REG_QUEUE_NUM, qsize);
    write32(VIRTIO_REG_QUEUE_RDY, 1);
}

void net_init() {
    write32(VIRTIO_REG_STATUS, 0);
    write32(VIRTIO_REG_STATUS, STATUS_ACKNOWLEDGE);
    write32(VIRTIO_REG_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER);

    uint32_t features = read32(VIRTIO_REG_DEV_FEAT);
    write32(VIRTIO_REG_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER | STATUS_FEATURES_OK);

    if (!(read32(VIRTIO_REG_STATUS) & STATUS_FEATURES_OK)) {
        printf("Virtio net rejected features!\n");
        write32(VIRTIO_REG_STATUS, STATUS_FAILED);
        return;
    }

    virtio_init_queue(0, &rxq);
    virtio_init_queue(1, &txq);
    for (int i = 0; i < 6; i++) {
        net_mac[i] = *(volatile uint8_t*)((uintptr_t)VIRTIO_NET_BASE + VIRTIO_REG_CONFIG + i);
    }

    write32(VIRTIO_REG_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER | STATUS_FEATURES_OK | STATUS_DRIVER_OK);
    
    printf("[NET] Virtio-Net Initialized! MAC: %x:%x:%x:%x:%x:%x\n", 
           net_mac[0], net_mac[1], net_mac[2], net_mac[3], net_mac[4], net_mac[5]);
}