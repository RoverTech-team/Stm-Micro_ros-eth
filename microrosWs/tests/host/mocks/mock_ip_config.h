#ifndef MOCK_IP_CONFIG_H
#define MOCK_IP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* IP address structure */
typedef struct {
    uint8_t octets[4];
} ip_address_t;

/* IP configuration structure */
typedef struct {
    ip_address_t ip;
    ip_address_t mask;
    ip_address_t gateway;
} ip_config_t;

/* IP validation functions */
bool ip_config_is_valid_octet(int octet);
bool ip_config_parse_ip(const char *ip_str, ip_address_t *ip);
bool ip_config_is_valid_subnet_mask(int cidr);
void ip_config_cidr_to_mask(int cidr, ip_address_t *mask);

/* Network calculations */
bool ip_config_is_network_address(const ip_address_t *ip, const ip_address_t *mask);
bool ip_config_is_broadcast_address(const ip_address_t *ip, const ip_address_t *mask);
void ip_config_get_network_address(const ip_address_t *ip, const ip_address_t *mask, ip_address_t *network);
void ip_config_get_broadcast_address(const ip_address_t *ip, const ip_address_t *mask, ip_address_t *broadcast);
bool ip_config_is_same_subnet(const ip_address_t *ip1, const ip_address_t *ip2, const ip_address_t *mask);

/* Mock control functions */
void mock_ip_config_reset(void);

#endif /* MOCK_IP_CONFIG_H */
