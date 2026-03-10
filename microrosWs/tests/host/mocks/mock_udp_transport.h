#ifndef MOCK_UDP_TRANSPORT_H
#define MOCK_UDP_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MOCK_UDP_PORT 8888
#define MOCK_MAX_UDP_PACKET_SIZE 2048

typedef enum {
    MOCK_UDP_STATE_CLOSED = 0,
    MOCK_UDP_STATE_OPEN,
    MOCK_UDP_STATE_ERROR
} mock_udp_state_t;

typedef struct {
    mock_udp_state_t state;
    int socket_fd;
    uint16_t port;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t write_count;
    uint32_t read_count;
    uint8_t last_error;
    bool bound;
    char remote_ip[16];
} mock_udp_transport_t;

typedef struct {
    bool socket_create_fail;
    bool bind_fail;
    bool sendto_fail;
    bool recv_fail;
    bool setsockopt_fail;
    int next_recv_timeout_ms;
    int next_sendto_result;
    int next_recv_result;
} mock_udp_error_config_t;

void mock_udp_transport_reset(void);
void mock_udp_transport_set_socket_create_fail(bool fail);
void mock_udp_transport_set_bind_fail(bool fail);
void mock_udp_transport_set_sendto_fail(bool fail);
void mock_udp_transport_set_recv_fail(bool fail);
void mock_udp_transport_set_setsockopt_fail(bool fail);
void mock_udp_transport_set_next_recv_timeout(int timeout_ms);
void mock_udp_transport_set_next_sendto_result(int result);
void mock_udp_transport_set_next_recv_result(int result);

mock_udp_transport_t* mock_udp_transport_get_instance(void);

bool mock_udp_transport_open(mock_udp_transport_t *transport, const char *ip_addr);
bool mock_udp_transport_close(mock_udp_transport_t *transport);
size_t mock_udp_transport_write(mock_udp_transport_t *transport, const uint8_t *buf, size_t len, uint8_t *err);
size_t mock_udp_transport_read(mock_udp_transport_t *transport, uint8_t *buf, size_t len, int timeout, uint8_t *err);

#endif
