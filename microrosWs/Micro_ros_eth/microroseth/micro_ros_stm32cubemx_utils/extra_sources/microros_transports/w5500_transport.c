#include <uxr/client/transport.h>

#include <rmw_microxrcedds_c/config.h>

#include "main.h"
#include "cmsis_os.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// --- WIZnet ioLibrary (W5500 hardwired TCP/IP) ---
#include "socket.h"
#include "wizchip_conf.h"

#ifdef RMW_UXRCE_TRANSPORT_CUSTOM

// --- micro-ROS W5500 UDP Transport ---
// Mirrors extra_sources/microros_transports/udp_transport.c, but every socket
// call goes to the W5500 over SPI instead of LwIP over the on-chip MAC.
// The uxrCustomTransport open/close/write/read signatures are identical, so
// main.c only changes the four function pointers passed to
// rmw_uros_set_custom_transport().
//
// Prerequisites (done by the application, NOT here):
//   1. wizchip_init() + network info (DHCP or static GAR/SUBR/SHAR/SIPR)
//      completed before rclc init.
//   2. wizchip_conf.c SPI callbacks wired to the HAL SPI handle on the W5500 bus.
//   3. ioLibrary Ethernet/ + Internet/ sources added to C_SOURCES.

#define W5500_TRANSPORT_SOCK   0
#define W5500_TRANSPORT_PORT   8888   // local source port for the XRCE session
#define W5500_AGENT_PORT       8888
#define W5500_MTU              1500

static uint8_t w5500_agent_ip[4] = {192, 168, 0, 1};
static bool w5500_opened = false;

// Minimal dotted-decimal parser (no inet_addr: LwIP may not be in the F4 build).
static bool w5500_parse_ip(const char * s, uint8_t out[4])
{
    if (s == NULL) return false;
    for (int i = 0; i < 4; i++)
    {
        unsigned val = 0;
        int digits = 0;
        while (*s >= '0' && *s <= '9')
        {
            val = val * 10u + (unsigned)(*s - '0');
            s++;
            digits++;
        }
        if (digits == 0 || val > 255u) return false;
        out[i] = (uint8_t)val;
        if (i < 3)
        {
            if (*s != '.') return false;
            s++;
        }
    }
    return *s == '\0';
}

bool cubemx_transport_open_w5500(struct uxrCustomTransport * transport)
{
    const char * ip_addr = (const char *)transport->args;

    if (!w5500_parse_ip(ip_addr, w5500_agent_ip))
    {
        printf("F4: w5500-open-bad-ip\r\n");
        return false;
    }

    if (w5500_opened)
    {
        close(W5500_TRANSPORT_SOCK);
        w5500_opened = false;
    }

    int8_t sn = socket(W5500_TRANSPORT_SOCK, Sn_MR_UDP, W5500_TRANSPORT_PORT, 0);
    if (sn != W5500_TRANSPORT_SOCK)
    {
        printf("F4: w5500-socket-failed sn=%d\r\n", (int)sn);
        return false;
    }
    w5500_opened = true;

    printf("F4: w5500-open-ok agent=%d.%d.%d.%d:%d\r\n",
        w5500_agent_ip[0], w5500_agent_ip[1],
        w5500_agent_ip[2], w5500_agent_ip[3], W5500_AGENT_PORT);
    return true;
}

bool cubemx_transport_close_w5500(struct uxrCustomTransport * transport)
{
    (void)transport;
    if (w5500_opened)
    {
        close(W5500_TRANSPORT_SOCK);
        w5500_opened = false;
    }
    return true;
}

size_t cubemx_transport_write_w5500(struct uxrCustomTransport * transport, const uint8_t * buf, size_t len, uint8_t * err)
{
    (void)transport;
    if (!w5500_opened)
    {
        printf("F4: w5500-write-no-socket\r\n");
        if (err) *err = 1;
        return 0;
    }
    if (len > W5500_MTU)
    {
        printf("F4: w5500-write-too-large %u\r\n", (unsigned)len);
        if (err) *err = 1;
        return 0;
    }

    int32_t ret = sendto(W5500_TRANSPORT_SOCK, (uint8_t *)buf, (uint16_t)len,
        w5500_agent_ip, W5500_AGENT_PORT);
    if (ret <= 0)
    {
        printf("F4: w5500-sendto-failed ret=%ld\r\n", (long)ret);
        if (err) *err = 1;
        return 0;
    }

    if (err) *err = 0;
    return (size_t)ret;
}

size_t cubemx_transport_read_w5500(struct uxrCustomTransport * transport, uint8_t * buf, size_t len, int timeout, uint8_t * err)
{
    (void)transport;
    if (!w5500_opened)
    {
        printf("F4: w5500-read-no-socket\r\n");
        if (err) *err = 1;
        return 0;
    }

    // W5500 has no SO_RCVTIMEO: poll RX_RSR until the deadline (1 ms quantum).
    uint32_t start = HAL_GetTick();
    while (getSn_RX_RSR(W5500_TRANSPORT_SOCK) == 0)
    {
        if ((HAL_GetTick() - start) >= (uint32_t)(timeout < 0 ? 0 : timeout))
        {
            if (err) *err = 0;   // timeout is not an error (matches udp_transport EAGAIN path)
            return 0;
        }
        osDelay(1);
    }

    uint8_t peer_ip[4];
    uint16_t peer_port = 0;
    int32_t ret = recvfrom(W5500_TRANSPORT_SOCK, buf,
        (uint16_t)(len > 0xFFFFu ? 0xFFFFu : len), peer_ip, &peer_port);
    if (ret < 0)
    {
        printf("F4: w5500-recvfrom-failed ret=%ld\r\n", (long)ret);
        if (err) *err = 1;
        return 0;
    }

    if (err) *err = 0;
    return (size_t)ret;
}

#endif
