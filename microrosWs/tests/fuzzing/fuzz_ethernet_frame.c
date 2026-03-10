#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>

#define ETHERNET_HDR_LEN 14
#define ETHERNET_MIN_LEN 64
#define ETHERNET_MAX_LEN 1518
#define ETHERNET_MTU 1500

#define ETH_P_IP  0x0800
#define ETH_P_ARP 0x0806
#define ETH_P_IPV6 0x86DD

#define IP_HDR_MIN_LEN 20
#define IP_HDR_MAX_LEN 60
#define UDP_HDR_LEN 8
#define TCP_HDR_MIN_LEN 20

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

typedef struct {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t ethertype;
} __attribute__((packed)) ethernet_header_t;

typedef struct {
    uint8_t version_ihl;
    uint8_t dscp_ecn;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_addr;
    uint32_t dst_addr;
} __attribute__((packed)) ip_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset_flags;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint8_t data[8];
} __attribute__((packed)) icmp_header_t;

typedef struct {
    ethernet_header_t eth;
    ip_header_t ip;
    udp_header_t udp;
    uint8_t payload[];
} __attribute__((packed)) ethernet_udp_frame_t;

typedef struct {
    bool valid_ethernet;
    bool valid_ip;
    bool valid_udp;
    bool valid_tcp;
    bool valid_icmp;
    bool truncated;
    bool oversized;
    bool malformed;
    bool checksum_error;
    size_t ethernet_payload_len;
    size_t ip_payload_len;
    size_t transport_payload_len;
} frame_parse_result_t;

static uint16_t compute_checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    
    while (len > 1) {
        sum += ((uint16_t)data[0] << 8) | data[1];
        data += 2;
        len -= 2;
    }
    
    if (len > 0) {
        sum += (uint16_t)data[0] << 8;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~(uint16_t)sum;
}

static uint16_t compute_ip_checksum(const ip_header_t *ip) {
    uint16_t stored_checksum = ip->checksum;
    ip_header_t *mut_ip = (ip_header_t *)ip;
    mut_ip->checksum = 0;
    
    uint16_t computed = compute_checksum((const uint8_t *)ip, ((ip->version_ihl & 0x0F) * 4));
    mut_ip->checksum = stored_checksum;
    
    return computed;
}

static bool validate_ethernet_header(const uint8_t *data, size_t len, frame_parse_result_t *result) {
    if (len < ETHERNET_HDR_LEN) {
        result->truncated = true;
        return false;
    }
    
    result->valid_ethernet = true;
    
    const ethernet_header_t *eth = (const ethernet_header_t *)data;
    uint16_t ethertype = ntohs(eth->ethertype);
    
    if (ethertype <= 1500) {
        result->malformed = true;
    }
    
    if (ethertype != ETH_P_IP && ethertype != ETH_P_ARP && 
        ethertype != ETH_P_IPV6 && ethertype < 0x05DC) {
        result->malformed = true;
    }
    
    result->ethernet_payload_len = len - ETHERNET_HDR_LEN;
    
    return true;
}

static bool validate_ip_header(const uint8_t *data, size_t len, frame_parse_result_t *result) {
    if (len < IP_HDR_MIN_LEN) {
        result->truncated = true;
        return false;
    }
    
    const ip_header_t *ip = (const ip_header_t *)data;
    
    uint8_t version = (ip->version_ihl >> 4) & 0x0F;
    uint8_t ihl = ip->version_ihl & 0x0F;
    
    if (version != 4) {
        result->malformed = true;
        if (version == 6) {
            return false;
        }
    }
    
    if (ihl < 5 || ihl > 15) {
        result->malformed = true;
        return false;
    }
    
    size_t header_len = ihl * 4;
    if (header_len > len) {
        result->truncated = true;
        return false;
    }
    
    uint16_t total_length = ntohs(ip->total_length);
    if (total_length < header_len || total_length > len) {
        result->malformed = true;
    }
    
    uint16_t computed_checksum = compute_ip_checksum(ip);
    if (computed_checksum != 0 && ip->checksum != 0) {
        result->checksum_error = true;
    }
    
    result->valid_ip = true;
    result->ip_payload_len = total_length - header_len;
    
    return true;
}

static bool validate_udp_header(const uint8_t *data, size_t len, frame_parse_result_t *result) {
    if (len < UDP_HDR_LEN) {
        result->truncated = true;
        return false;
    }
    
    const udp_header_t *udp = (const udp_header_t *)data;
    
    uint16_t udp_length = ntohs(udp->length);
    
    if (udp_length < UDP_HDR_LEN) {
        result->malformed = true;
    }
    
    if (udp_length > len) {
        result->truncated = true;
    }
    
    if (udp->checksum != 0) {
        result->checksum_error = true;
    }
    
    result->valid_udp = true;
    result->transport_payload_len = (udp_length >= UDP_HDR_LEN) ? 
                                    (udp_length - UDP_HDR_LEN) : 0;
    
    return true;
}

static bool validate_tcp_header(const uint8_t *data, size_t len, frame_parse_result_t *result) {
    if (len < TCP_HDR_MIN_LEN) {
        result->truncated = true;
        return false;
    }
    
    const tcp_header_t *tcp = (const tcp_header_t *)data;
    
    uint8_t data_offset = (tcp->data_offset_flags >> 4) & 0x0F;
    
    if (data_offset < 5 || data_offset > 15) {
        result->malformed = true;
    }
    
    size_t header_len = data_offset * 4;
    if (header_len > len) {
        result->truncated = true;
    }
    
    result->valid_tcp = true;
    result->transport_payload_len = (len > header_len) ? (len - header_len) : 0;
    
    return true;
}

static bool validate_icmp_header(const uint8_t *data, size_t len, frame_parse_result_t *result) {
    if (len < 4) {
        result->truncated = true;
        return false;
    }
    
    const icmp_header_t *icmp = (const icmp_header_t *)data;
    
    if (icmp->type > 44 && icmp->type != 255) {
        result->malformed = true;
    }
    
    result->valid_icmp = true;
    result->transport_payload_len = len - 4;
    
    return true;
}

static void fuzz_parse_ethernet_frame(const uint8_t *data, size_t len) {
    frame_parse_result_t result = {0};
    
    if (len > ETHERNET_MAX_LEN) {
        result.oversized = true;
    }
    
    if (!validate_ethernet_header(data, len, &result)) {
        return;
    }
    
    const ethernet_header_t *eth = (const ethernet_header_t *)data;
    uint16_t ethertype = ntohs(eth->ethertype);
    
    if (ethertype != ETH_P_IP) {
        return;
    }
    
    const uint8_t *ip_data = data + ETHERNET_HDR_LEN;
    size_t ip_len = len - ETHERNET_HDR_LEN;
    
    if (!validate_ip_header(ip_data, ip_len, &result)) {
        return;
    }
    
    const ip_header_t *ip = (const ip_header_t *)ip_data;
    size_t ip_header_len = (ip->version_ihl & 0x0F) * 4;
    const uint8_t *transport_data = ip_data + ip_header_len;
    size_t transport_len = 0;
    
    if (ntohs(ip->total_length) > ip_header_len) {
        transport_len = ntohs(ip->total_length) - ip_header_len;
    }
    
    if (transport_len > (len - ETHERNET_HDR_LEN - ip_header_len)) {
        transport_len = len - ETHERNET_HDR_LEN - ip_header_len;
    }
    
    switch (ip->protocol) {
        case IP_PROTO_UDP:
            validate_udp_header(transport_data, transport_len, &result);
            break;
        case IP_PROTO_TCP:
            validate_tcp_header(transport_data, transport_len, &result);
            break;
        case IP_PROTO_ICMP:
            validate_icmp_header(transport_data, transport_len, &result);
            break;
        default:
            break;
    }
}

static void fuzz_truncated_frames(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < (len < 20 ? len : 20); i++) {
        fuzz_parse_ethernet_frame(data, i);
    }
}

static void fuzz_oversized_frames(const uint8_t *data, size_t len) {
    if (len < ETHERNET_MAX_LEN) return;
    
    uint8_t oversized_buffer[ETHERNET_MAX_LEN * 2];
    memcpy(oversized_buffer, data, len > sizeof(oversized_buffer) ? 
           sizeof(oversized_buffer) : len);
    
    fuzz_parse_ethernet_frame(oversized_buffer, sizeof(oversized_buffer));
}

static void fuzz_malformed_ethertypes(const uint8_t *data, size_t len) {
    if (len < ETHERNET_HDR_LEN) return;
    
    uint8_t frame_buffer[ETHERNET_MAX_LEN];
    memcpy(frame_buffer, data, len > sizeof(frame_buffer) ? 
           sizeof(frame_buffer) : len);
    
    ethernet_header_t *eth = (ethernet_header_t *)frame_buffer;
    
    uint16_t test_ethertypes[] = {
        0x0000, 0x0001, 0x05DC, 0x05DD,
        0x0800, 0x0806, 0x86DD,
        0xFFFF, 0x8035, 0x809B
    };
    
    for (size_t i = 0; i < sizeof(test_ethertypes) / sizeof(test_ethertypes[0]); i++) {
        eth->ethertype = htons(test_ethertypes[i]);
        fuzz_parse_ethernet_frame(frame_buffer, len);
    }
}

static void fuzz_ip_header_variants(const uint8_t *data, size_t len) {
    if (len < ETHERNET_HDR_LEN + IP_HDR_MIN_LEN) return;
    
    uint8_t frame_buffer[ETHERNET_MAX_LEN];
    memcpy(frame_buffer, data, len > sizeof(frame_buffer) ? 
           sizeof(frame_buffer) : len);
    
    ethernet_header_t *eth = (ethernet_header_t *)frame_buffer;
    eth->ethertype = htons(ETH_P_IP);
    
    ip_header_t *ip = (ip_header_t *)(frame_buffer + ETHERNET_HDR_LEN);
    
    for (uint8_t version = 0; version <= 15; version++) {
        for (uint8_t ihl = 0; ihl <= 15; ihl++) {
            ip->version_ihl = (version << 4) | ihl;
            fuzz_parse_ethernet_frame(frame_buffer, len);
        }
    }
    
    uint16_t invalid_lengths[] = {0, 1, 19, 65535, 0xFFFF};
    for (size_t i = 0; i < sizeof(invalid_lengths) / sizeof(invalid_lengths[0]); i++) {
        ip->total_length = htons(invalid_lengths[i]);
        fuzz_parse_ethernet_frame(frame_buffer, len);
    }
}

static void fuzz_udp_header_variants(const uint8_t *data, size_t len) {
    if (len < ETHERNET_HDR_LEN + IP_HDR_MIN_LEN + UDP_HDR_LEN) return;
    
    uint8_t frame_buffer[ETHERNET_MAX_LEN];
    memcpy(frame_buffer, data, len > sizeof(frame_buffer) ? 
           sizeof(frame_buffer) : len);
    
    ethernet_header_t *eth = (ethernet_header_t *)frame_buffer;
    eth->ethertype = htons(ETH_P_IP);
    
    ip_header_t *ip = (ip_header_t *)(frame_buffer + ETHERNET_HDR_LEN);
    ip->version_ihl = 0x45;
    ip->total_length = htons(len - ETHERNET_HDR_LEN);
    ip->protocol = IP_PROTO_UDP;
    
    udp_header_t *udp = (udp_header_t *)(frame_buffer + ETHERNET_HDR_LEN + IP_HDR_MIN_LEN);
    
    uint16_t invalid_lengths[] = {0, 1, 7, 65535, 0xFFFF};
    for (size_t i = 0; i < sizeof(invalid_lengths) / sizeof(invalid_lengths[0]); i++) {
        udp->length = htons(invalid_lengths[i]);
        fuzz_parse_ethernet_frame(frame_buffer, len);
    }
    
    uint16_t checksums[] = {0, 0xFFFF, 0x1234, 0xABCD};
    for (size_t i = 0; i < sizeof(checksums) / sizeof(checksums[0]); i++) {
        udp->checksum = htons(checksums[i]);
        fuzz_parse_ethernet_frame(frame_buffer, len);
    }
}

static void fuzz_checksum_validation(const uint8_t *data, size_t len) {
    if (len < ETHERNET_HDR_LEN + IP_HDR_MIN_LEN) return;
    
    uint8_t frame_buffer[ETHERNET_MAX_LEN];
    memcpy(frame_buffer, data, len > sizeof(frame_buffer) ? 
           sizeof(frame_buffer) : len);
    
    ethernet_header_t *eth = (ethernet_header_t *)frame_buffer;
    eth->ethertype = htons(ETH_P_IP);
    
    ip_header_t *ip = (ip_header_t *)(frame_buffer + ETHERNET_HDR_LEN);
    ip->version_ihl = 0x45;
    ip->total_length = htons(len - ETHERNET_HDR_LEN);
    ip->protocol = IP_PROTO_UDP;
    
    ip->checksum = 0;
    fuzz_parse_ethernet_frame(frame_buffer, len);
    
    ip->checksum = 0xFFFF;
    fuzz_parse_ethernet_frame(frame_buffer, len);
    
    ip->checksum = compute_checksum((uint8_t *)ip, IP_HDR_MIN_LEN);
    fuzz_parse_ethernet_frame(frame_buffer, len);
    
    ip->checksum = ~compute_checksum((uint8_t *)ip, IP_HDR_MIN_LEN);
    fuzz_parse_ethernet_frame(frame_buffer, len);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len) {
    if (len == 0) {
        return 0;
    }
    
    fuzz_parse_ethernet_frame(data, len);
    
    fuzz_truncated_frames(data, len);
    
    fuzz_oversized_frames(data, len);
    
    fuzz_malformed_ethertypes(data, len);
    
    fuzz_ip_header_variants(data, len);
    
    fuzz_udp_header_variants(data, len);
    
    fuzz_checksum_validation(data, len);
    
    return 0;
}