#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "net.h"
#include "pmm.h"

#define STATUS_ACKNOWLEDGE  1
#define STATUS_DRIVER       2
#define STATUS_FEATURES_OK  8
#define STATUS_DRIVER_OK    4
#define STATUS_FAILED       128
#define VIRTIO_REG_DEV_FEAT_SEL 0x014
#define VIRTIO_REG_DRV_FEAT_SEL 0x024
#define VIRTIO_REG_DRV_FEAT     0x020

#define VIRTIO_REG_CONFIG   0x100
#define VIRTIO_REG_QUEUE_NOTIFY 0x050
#define VIRTIO_REG_INTERRUPT_STATUS 0x060
#define VIRTIO_REG_INTERRUPT_ACK    0x064

#define VIRTQ_DESC_F_NEXT   1
#define VIRTQ_DESC_F_WRITE  2

#define read32(offset)      (*(volatile uint32_t*)(VIRTIO_NET_BASE + (offset)))
#define write32(offset, val) (*(volatile uint32_t*)(VIRTIO_NET_BASE + (offset)) = (val))

#define htons ntohs // Byte swap is symmetric for 16-bit integers

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
    uint32_t qsize;
    uint16_t last_used_idx;
} virtq;

typedef struct virtio_net_config {
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
} __attribute__((packed)) virtio_net_config;

typedef struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed)) virtio_net_hdr;

typedef struct ethernet_frame {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype; // 0x0806 for ARP, 0x0800 for IPv4
    uint8_t payload[];
} __attribute__((packed)) ethernet_frame;

typedef struct arp_packet {
    uint16_t hw_type;      // 1 for Ethernet
    uint16_t proto_type;   // 0x0800 for IPv4
    uint8_t  hw_len;       // 6 for MAC
    uint8_t  proto_len;    // 4 for IPv4
    uint16_t opcode;       // 1 for Request, 2 for Reply
    uint8_t  sender_mac[6];
    uint8_t  sender_ip[4];
    uint8_t  target_mac[6];
    uint8_t  target_ip[4];
} __attribute__((packed)) arp_packet;

typedef struct ipv4_packet {
    uint8_t version_ihl;
    uint8_t dscp_ecn;
    uint16_t length;
    uint16_t ident;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;       // 1 = ICMP, 6 = TCP, 17 = UDP
    uint16_t checksum;
    uint8_t src_ip[4];
    uint8_t dst_ip[4];
    uint8_t payload[];
} __attribute__((packed)) ipv4_packet;

typedef struct icmp_packet {
    uint8_t type;           // 8 = Echo Request (Ping), 0 = Echo Reply
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
    uint8_t data[];
} __attribute__((packed)) icmp_packet;

static virtq rxq;
static virtq txq;
static uint8_t net_mac[6];
static uint8_t my_ip[4] = {10, 0, 2, 15};   // 10.0.2.15 (Standard QEMU user-net IP)

static uint16_t tx_desc_idx = 0;
static uint32_t tx_qsize = 256;

static inline uint16_t ntohs(uint16_t netshort) {
    return (netshort >> 8) | (netshort << 8);
}

static uint16_t calculate_checksum(void *vdata, uint32_t length) {
    uint8_t *data = (uint8_t *)vdata;
    uint32_t acc = 0;

    for (uint32_t i = 0; i < (length / 2); i++) {
        acc += (data[i * 2] << 8) | data[i * 2 + 1];
    }
    if (length % 2) {
        acc += (data[length - 1] << 8);
    }
    while (acc >> 16) {
        acc = (acc & 0xffff) + (acc >> 16);
    }
    return ~acc;
}

static void virtio_init_queue(uint32_t queue_index, virtq* queue) {
    write32(VIRTIO_REG_QUEUE_SEL, queue_index);
    
    uint32_t qsize = read32(VIRTIO_REG_QUEUE_MAX);
    if (qsize == 0) {
        printf("Queue %d is unavailable!\n", queue_index);
        return;
    }

    // 1024 descriptors * 16 bytes = 16,384 bytes (4 pages)
    pmm_alloc_frame();
    pmm_alloc_frame();
    pmm_alloc_frame();
    uintptr_t desc_phys = pmm_alloc_frame();

    // 1024 avail elements * 2 bytes + 4 byte header = 2,052 bytes (1 page)
    uintptr_t avail_phys = pmm_alloc_frame(); 

    // 1024 used elements * 8 bytes + 4 byte header = 8,196 bytes (3 pages)
    pmm_alloc_frame();
    pmm_alloc_frame();
    uintptr_t used_phys = pmm_alloc_frame();

    memset((void*)desc_phys, 0, 4 * 4096);
    memset((void*)avail_phys, 0, 1 * 4096);
    memset((void*)used_phys, 0, 3 * 4096);

    queue->desc = (virtq_desc*)desc_phys;
    queue->avail = (virtq_avail*)avail_phys;
    queue->used = (virtq_used*)used_phys;
    queue->qsize = qsize;
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

static void net_rx_fill() {
    for (int i = 0; i < rxq.qsize; i++) {
        uintptr_t phys = pmm_alloc_frame();
        if (!phys) {
            printf("[NET] Out of memory for RX buffers!\n");
            break;
        }

        rxq.desc[i].addr = phys;
        rxq.desc[i].len = 4096;
        rxq.desc[i].flags = VIRTQ_DESC_F_WRITE;
        rxq.desc[i].next = 0;

        rxq.avail->ring[i % rxq.qsize] = i;
    }

    __sync_synchronize();
    rxq.avail->idx = rxq.qsize;
    __sync_synchronize();
    
    write32(VIRTIO_REG_QUEUE_NOTIFY, 0);
}

void net_init() {
    write32(VIRTIO_REG_STATUS, 0);
    write32(VIRTIO_REG_STATUS, STATUS_ACKNOWLEDGE);
    write32(VIRTIO_REG_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER);

    write32(VIRTIO_REG_DEV_FEAT_SEL, 0);
    uint32_t features_low = read32(VIRTIO_REG_DEV_FEAT) & 0; 
    write32(VIRTIO_REG_DRV_FEAT_SEL, 0);
    write32(VIRTIO_REG_DRV_FEAT, features_low);
    write32(VIRTIO_REG_DEV_FEAT_SEL, 1);
    uint32_t features_high = read32(VIRTIO_REG_DEV_FEAT) & 1; 
    write32(VIRTIO_REG_DRV_FEAT_SEL, 1);
    write32(VIRTIO_REG_DRV_FEAT, features_high);

    write32(VIRTIO_REG_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER | STATUS_FEATURES_OK);

    if (!(read32(VIRTIO_REG_STATUS) & STATUS_FEATURES_OK)) {
        printf("Virtio net rejected features!\n");
        write32(VIRTIO_REG_STATUS, STATUS_FAILED);
        return;
    }

    virtio_init_queue(0, &rxq);
    virtio_init_queue(1, &txq);
    net_rx_fill();
    for (int i = 0; i < 6; i++) {
        net_mac[i] = *(volatile uint8_t*)((uintptr_t)VIRTIO_NET_BASE + VIRTIO_REG_CONFIG + i);
    }

    write32(VIRTIO_REG_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER | STATUS_FEATURES_OK | STATUS_DRIVER_OK);
    
    printf("[NET] Virtio-Net Initialized! MAC: %x:%x:%x:%x:%x:%x\n", 
           net_mac[0], net_mac[1], net_mac[2], net_mac[3], net_mac[4], net_mac[5]);
}

static void net_send(void *mac_frame, uint32_t len) {
    uintptr_t phys = pmm_alloc_frame();
    if (!phys) {
        return;
    }

    virtio_net_hdr *hdr = (virtio_net_hdr*)phys;
    memset((void*)phys, 0, sizeof(virtio_net_hdr));

    void *dst = (void*)(phys + sizeof(virtio_net_hdr));
    memcpy((void*)dst, mac_frame, len);

    uint16_t head = tx_desc_idx % txq.qsize;
    txq.desc[head].addr = phys;
    txq.desc[head].len = sizeof(virtio_net_hdr) + len;
    txq.desc[head].flags = 0;
    txq.desc[head].next = 0;

    __sync_synchronize();

    uint16_t avail = txq.avail->idx;
    txq.avail->ring[avail % txq.qsize] = head;

    __sync_synchronize();

    txq.avail->idx = avail + 1;
    
    __sync_synchronize();

    write32(VIRTIO_REG_QUEUE_NOTIFY, 1);
    tx_desc_idx++;
}

static void arp_handle(void *payload, uint32_t len, uint8_t *src_mac) {
    if (len < sizeof(arp_packet)) {
        return;
    }

    arp_packet *arp = (arp_packet *)payload;
    uint16_t opcode = ntohs(arp->opcode);

    if (opcode == 1 && memcmp(arp->target_ip, my_ip, 4) == 0) {
        uint32_t reply_len = sizeof(ethernet_frame) + sizeof(arp_packet);
        uint8_t reply_buffer[reply_len];
        
        ethernet_frame *eth_reply = (ethernet_frame *)reply_buffer;
        memcpy(eth_reply->dst_mac, src_mac, 6);
        memcpy(eth_reply->src_mac, net_mac, 6);
        eth_reply->ethertype = ntohs(0x0806);

        arp_packet *arp_reply = (arp_packet *)eth_reply->payload;
        arp_reply->hw_type = ntohs(1);
        arp_reply->proto_type = ntohs(0x0800);
        arp_reply->hw_len = 6;
        arp_reply->proto_len = 4;
        arp_reply->opcode = ntohs(2);

        memcpy(arp_reply->sender_mac, net_mac, 6);
        memcpy(arp_reply->sender_ip, my_ip, 4);
        memcpy(arp_reply->target_mac, arp->sender_mac, 6);
        memcpy(arp_reply->target_ip, arp->sender_ip, 4);

        net_send(reply_buffer, reply_len);
    }
}

static void ipv4_handle(void *payload, uint32_t len, uint8_t *src_mac) {
    if (len < sizeof(ipv4_packet)) {
        return;
    }

    ipv4_packet *ip = (ipv4_packet *)payload;
    if (memcmp(ip->dst_ip, my_ip, 4) != 0) {
        return;
    }

    if (ip->protocol == 1) {
        icmp_packet *icmp = (icmp_packet *)ip->payload;
        if (icmp->type == 8) {
            icmp->type = 0;
            icmp->checksum = 0;
            
            uint32_t icmp_len = ntohs(ip->length) - (ip->version_ihl & 0x0F) * 4;
            icmp->checksum = htons(calculate_checksum(icmp, icmp_len));

            uint8_t temp_ip[4];
            memcpy(temp_ip, ip->src_ip, 4);
            memcpy(ip->src_ip, my_ip, 4);
            memcpy(ip->dst_ip, temp_ip, 4);

            uint32_t reply_len = sizeof(ethernet_frame) + ntohs(ip->length);
            uint8_t reply_buffer[reply_len];
            ethernet_frame *eth_reply = (ethernet_frame *)reply_buffer;
            
            memcpy(eth_reply->dst_mac, src_mac, 6);
            memcpy(eth_reply->src_mac, net_mac, 6);
            eth_reply->ethertype = ntohs(0x0800);   // IPv4
            
            memcpy(eth_reply->payload, ip, ntohs(ip->length));

            net_send(reply_buffer, reply_len);
        }
    }
}


static void ethernet_handle_packet(void *packet, uint32_t len) {
    if (len < sizeof(ethernet_frame)) {
        return;
    }

    ethernet_frame *eth = (ethernet_frame *)packet;
    uint16_t type = ntohs(eth->ethertype);

    if (type == 0x0806) { // ARP
        arp_handle(eth->payload, len - sizeof(ethernet_frame), eth->src_mac);
    } else if (type == 0x0800) { // IPv4
        ipv4_handle(eth->payload, len - sizeof(ethernet_frame), eth->src_mac);
    } else {
        printf("Unknown EtherType: 0x%04x\n", type);
    }
}

void net_poll_rx() {
    bool received = false;

    while (rxq.last_used_idx != rxq.used->idx) {
        __sync_synchronize();

        uint16_t used_ring_idx = rxq.last_used_idx % rxq.qsize;
        uint32_t desc_id = rxq.used->ring[used_ring_idx].id;
        uint32_t total_len = rxq.used->ring[used_ring_idx].len;

        virtio_net_hdr *hdr = (virtio_net_hdr *)rxq.desc[desc_id].addr;
        void *packet = (void *)((uintptr_t)hdr + sizeof(virtio_net_hdr));
        uint32_t packet_len = total_len - sizeof(virtio_net_hdr);

        if (packet_len > 0) {
            ethernet_handle_packet(packet, packet_len);
        }

        memset(hdr, 0, sizeof(virtio_net_hdr));
        uint16_t avail_idx = rxq.avail->idx;
        rxq.avail->ring[avail_idx % rxq.qsize] = desc_id;
        
        __sync_synchronize();
        rxq.avail->idx = avail_idx + 1;

        rxq.last_used_idx++;
        received = true;
    }

    if (received) {
        __sync_synchronize();
        write32(VIRTIO_REG_QUEUE_NOTIFY, 0);
    }
}

void net_cleanup_tx() {
    while (txq.last_used_idx != txq.used->idx) {
        __sync_synchronize();

        uint16_t used_ring_idx = txq.last_used_idx % txq.qsize;
        uint32_t desc_id = txq.used->ring[used_ring_idx].id;

        uintptr_t phys = txq.desc[desc_id].addr;
        pmm_free_frame(phys); 

        txq.last_used_idx++;
    }
}

void arp_request_gateway() {
    uint8_t gateway_ip[4] = {10, 0, 2, 2};
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    uint32_t req_len = sizeof(ethernet_frame) + sizeof(arp_packet);
    uint8_t req_buffer[req_len];

    ethernet_frame *eth = (ethernet_frame *)req_buffer;
    memcpy(eth->dst_mac, broadcast_mac, 6);
    memcpy(eth->src_mac, net_mac, 6);
    eth->ethertype = ntohs(0x0806);

    arp_packet *arp = (arp_packet *)eth->payload;
    arp->hw_type = ntohs(1);
    arp->proto_type = ntohs(0x0800);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = ntohs(1);

    memcpy(arp->sender_mac, net_mac, 6);
    memcpy(arp->sender_ip, my_ip, 4);
    memset(arp->target_mac, 0, 6);
    memcpy(arp->target_ip, gateway_ip, 4);

    net_send(req_buffer, req_len);
    printf("[NET] Sent ARP Request for Gateway (10.0.2.2)...\n");
}

void virtio_net_handle_irq() {
    uint32_t status = read32(VIRTIO_REG_INTERRUPT_STATUS);
    write32(VIRTIO_REG_INTERRUPT_ACK, status);
    
    if (status & 1) {
        net_poll_rx();
        net_cleanup_tx();
    }
}