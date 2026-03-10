#include "mock_ip_config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void mock_ip_config_reset(void) {
    /* No internal state to reset for now */
}

bool ip_config_is_valid_octet(int octet) {
    return (octet >= 0 && octet <= 255);
}

bool ip_config_parse_ip(const char *ip_str, ip_address_t *ip) {
    if (ip_str == NULL || ip == NULL) {
        return false;
    }
    
    int octets[4];
    int count = sscanf(ip_str, "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]);
    
    if (count != 4) {
        return false;
    }
    
    /* Check for extra octets - look for more dots after the 4th octet */
    const char *ptr = ip_str;
    int dot_count = 0;
    while (*ptr) {
        if (*ptr == '.') dot_count++;
        ptr++;
    }
    if (dot_count != 3) {
        return false;  /* Too many or too few dots */
    }
    
    for (int i = 0; i < 4; i++) {
        if (!ip_config_is_valid_octet(octets[i])) {
            return false;
        }
        ip->octets[i] = (uint8_t)octets[i];
    }
    
    return true;
}

bool ip_config_is_valid_subnet_mask(int cidr) {
    return (cidr >= 1 && cidr <= 32);
}

void ip_config_cidr_to_mask(int cidr, ip_address_t *mask) {
    if (mask == NULL) {
        return;
    }
    
    memset(mask, 0, sizeof(ip_address_t));
    
    if (cidr < 1 || cidr > 32) {
        return;
    }
    
    /* Set bits from left to right */
    uint32_t mask_value = 0xFFFFFFFF << (32 - cidr);
    
    mask->octets[0] = (mask_value >> 24) & 0xFF;
    mask->octets[1] = (mask_value >> 16) & 0xFF;
    mask->octets[2] = (mask_value >> 8) & 0xFF;
    mask->octets[3] = mask_value & 0xFF;
}

bool ip_config_is_network_address(const ip_address_t *ip, const ip_address_t *mask) {
    if (ip == NULL || mask == NULL) {
        return false;
    }
    
    /* Network address has host portion all zeros */
    for (int i = 0; i < 4; i++) {
        if ((ip->octets[i] & ~mask->octets[i]) != 0) {
            return false;
        }
    }
    
    return true;
}

bool ip_config_is_broadcast_address(const ip_address_t *ip, const ip_address_t *mask) {
    if (ip == NULL || mask == NULL) {
        return false;
    }
    
    /* Broadcast address has host portion all ones */
    for (int i = 0; i < 4; i++) {
        uint8_t host_part = ip->octets[i] & ~mask->octets[i];
        if (host_part != (~mask->octets[i] & 0xFF)) {
            return false;
        }
    }
    
    return true;
}

void ip_config_get_network_address(const ip_address_t *ip, const ip_address_t *mask, ip_address_t *network) {
    if (ip == NULL || mask == NULL || network == NULL) {
        return;
    }
    
    for (int i = 0; i < 4; i++) {
        network->octets[i] = ip->octets[i] & mask->octets[i];
    }
}

void ip_config_get_broadcast_address(const ip_address_t *ip, const ip_address_t *mask, ip_address_t *broadcast) {
    if (ip == NULL || mask == NULL || broadcast == NULL) {
        return;
    }
    
    for (int i = 0; i < 4; i++) {
        broadcast->octets[i] = ip->octets[i] | ~mask->octets[i];
    }
}

bool ip_config_is_same_subnet(const ip_address_t *ip1, const ip_address_t *ip2, const ip_address_t *mask) {
    if (ip1 == NULL || ip2 == NULL || mask == NULL) {
        return false;
    }
    
    for (int i = 0; i < 4; i++) {
        if ((ip1->octets[i] & mask->octets[i]) != (ip2->octets[i] & mask->octets[i])) {
            return false;
        }
    }
    
    return true;
}
