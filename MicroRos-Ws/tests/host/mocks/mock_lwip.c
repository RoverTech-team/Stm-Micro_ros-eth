#include "mock_lwip.h"
#include <string.h>

/* Global default netif */
netif_t* netif_default = NULL;

/* Internal mock state */
static bool netif_up = false;
static bool link_up = false;

void mock_lwip_reset(void) {
    netif_up = false;
    link_up = false;
    netif_default = NULL;
}

void mock_lwip_set_netif_up(bool up) {
    netif_up = up;
}

void mock_lwip_set_link_up(bool up) {
    link_up = up;
}

bool netif_is_up(struct netif *netif) {
    (void)netif; /* Unused in mock */
    return netif_up;
}

bool netif_is_link_up(struct netif *netif) {
    (void)netif; /* Unused in mock */
    return link_up;
}

err_t etharp_request(struct netif *netif, const ip_addr_t *ipaddr) {
    (void)netif;   /* Unused in mock */
    (void)ipaddr;  /* Unused in mock */
    return ERR_OK;
}
