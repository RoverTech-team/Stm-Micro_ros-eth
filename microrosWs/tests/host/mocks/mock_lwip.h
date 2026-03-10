#ifndef MOCK_LWIP_H
#define MOCK_LWIP_H

#include <stdint.h>
#include <stdbool.h>

#define NETIF_FLAG_UP         0x01
#define NETIF_FLAG_BROADCAST  0x02
#define NETIF_FLAG_LINK_UP    0x04
#define NETIF_FLAG_ETHARP     0x08
#define NETIF_FLAG_IGMP       0x10

typedef struct {
    uint32_t addr[4];
} ip_addr_t;

typedef struct netif {
    uint8_t flags;
    ip_addr_t ip_addr;
    ip_addr_t netmask;
    ip_addr_t gw;
    bool link_up;
    char name[2];
    uint8_t num;
    void *state;
} netif_t;

typedef int8_t err_t;

#define ERR_OK    0
#define ERR_MEM  -1
#define ERR_IF   -2
#define ERR_RTE  -3

extern netif_t* netif_default;

bool netif_is_up(struct netif *netif);
bool netif_is_link_up(struct netif *netif);
err_t etharp_request(struct netif *netif, const ip_addr_t *ipaddr);

/* Mock control functions */
void mock_lwip_reset(void);
void mock_lwip_set_netif_up(bool up);
void mock_lwip_set_link_up(bool up);

#endif /* MOCK_LWIP_H */
