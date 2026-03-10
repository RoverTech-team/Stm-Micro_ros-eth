#include <uxr/client/transport.h>

#include <rmw_microxrcedds_c/config.h>

#include "main.h"
#include "cmsis_os.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// --- LWIP ---
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include <lwip/sockets.h>

#ifdef RMW_UXRCE_TRANSPORT_CUSTOM

// --- micro-ROS Transports ---
#define UDP_PORT        8888
static int sock_fd = -1;

bool cubemx_transport_open(struct uxrCustomTransport * transport){
    const char * ip_addr = (const char*) transport->args;

    if (ip_addr == NULL) {
        printf("CM7: udp-open-no-ip\r\n");
        return false;
    }

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sock_fd < 0) {
        printf("CM7: udp-open-socket-failed\r\n");
        return false;  // Socket creation failed
    }
    
    struct sockaddr_in agent_addr;
    memset(&agent_addr, 0, sizeof(agent_addr));
    agent_addr.sin_family = AF_INET;
    agent_addr.sin_port = htons(UDP_PORT);
    agent_addr.sin_addr.s_addr = inet_addr(ip_addr);

    if (connect(sock_fd, (struct sockaddr *)&agent_addr, sizeof(agent_addr)) == -1)
    {
        printf("CM7: udp-connect-failed\r\n");
        closesocket(sock_fd);
        sock_fd = -1;
        return false;
    }

    printf("CM7: udp-open-ok port=%d agent=%s\r\n", UDP_PORT, ip_addr);

    return true;
}

bool cubemx_transport_close(struct uxrCustomTransport * transport){
    (void)transport;  // Unused parameter
    if (sock_fd >= 0)
    {
        closesocket(sock_fd);
        sock_fd = -1;
    }
    return true;
}

size_t cubemx_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err){
    (void)transport;
    if (sock_fd < 0)
    {
        printf("CM7: udp-write-no-socket\r\n");
        if (err) *err = 1;
        return 0;
    }

    int ret = send(sock_fd, buf, len, 0);
    
    if (ret < 0) {
        printf("CM7: udp-sendto-failed\r\n");
        if (err) *err = 1;
        return 0;
    }
    
    if (err) *err = 0;
    return (size_t)ret;
}

size_t cubemx_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err){
    (void)transport;
    if (sock_fd < 0) {
        printf("CM7: udp-read-no-socket\r\n");
        if (err) *err = 1;
        return 0;
    }

    // Set timeout
    struct timeval tv_out;
    tv_out.tv_sec = timeout / 1000;
    tv_out.tv_usec = (timeout % 1000) * 1000;
    
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out)) < 0) {
        printf("CM7: udp-setsockopt-failed\r\n");
        if (err) *err = 1;
        return 0;
    }
    
    int ret = recv(sock_fd, buf, len, 0);
    
    if (ret < 0) {
        printf("CM7: udp-recv-timeout-or-failed\r\n");
        if (err) *err = 1;
        return 0;
    }
    
    if (err) *err = 0;
    return (size_t)ret;
}

#endif
