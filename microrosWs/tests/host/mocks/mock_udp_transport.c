#include "mock_udp_transport.h"
#include <string.h>
#include <stdlib.h>

static mock_udp_transport_t transport_instance = {0};
static mock_udp_error_config_t error_config = {0};
static uint8_t mock_receive_buffer[MOCK_MAX_UDP_PACKET_SIZE];
static size_t mock_receive_buffer_len = 0;

void mock_udp_transport_reset(void) {
    memset(&transport_instance, 0, sizeof(transport_instance));
    memset(&error_config, 0, sizeof(error_config));
    transport_instance.socket_fd = -1;
    transport_instance.state = MOCK_UDP_STATE_CLOSED;
    transport_instance.port = MOCK_UDP_PORT;
    memset(mock_receive_buffer, 0, sizeof(mock_receive_buffer));
    mock_receive_buffer_len = 0;
}

void mock_udp_transport_set_socket_create_fail(bool fail) {
    error_config.socket_create_fail = fail;
}

void mock_udp_transport_set_bind_fail(bool fail) {
    error_config.bind_fail = fail;
}

void mock_udp_transport_set_sendto_fail(bool fail) {
    error_config.sendto_fail = fail;
}

void mock_udp_transport_set_recv_fail(bool fail) {
    error_config.recv_fail = fail;
}

void mock_udp_transport_set_setsockopt_fail(bool fail) {
    error_config.setsockopt_fail = fail;
}

void mock_udp_transport_set_next_recv_timeout(int timeout_ms) {
    error_config.next_recv_timeout_ms = timeout_ms;
}

void mock_udp_transport_set_next_sendto_result(int result) {
    error_config.next_sendto_result = result;
}

void mock_udp_transport_set_next_recv_result(int result) {
    error_config.next_recv_result = result;
}

mock_udp_transport_t* mock_udp_transport_get_instance(void) {
    return &transport_instance;
}

bool mock_udp_transport_open(mock_udp_transport_t *transport, const char *ip_addr) {
    if (transport == NULL) {
        return false;
    }
    
    if (error_config.socket_create_fail) {
        transport->state = MOCK_UDP_STATE_ERROR;
        transport->last_error = 1;
        return false;
    }
    
    transport->socket_fd = 42;
    transport->state = MOCK_UDP_STATE_OPEN;
    
    if (error_config.bind_fail) {
        transport->socket_fd = -1;
        transport->state = MOCK_UDP_STATE_ERROR;
        transport->last_error = 2;
        return false;
    }
    
    transport->bound = true;
    transport->port = MOCK_UDP_PORT;
    
    if (ip_addr != NULL) {
        strncpy(transport->remote_ip, ip_addr, sizeof(transport->remote_ip) - 1);
        transport->remote_ip[sizeof(transport->remote_ip) - 1] = '\0';
    }
    
    transport->bytes_sent = 0;
    transport->bytes_received = 0;
    transport->write_count = 0;
    transport->read_count = 0;
    
    return true;
}

bool mock_udp_transport_close(mock_udp_transport_t *transport) {
    if (transport == NULL) {
        return false;
    }
    
    if (transport->socket_fd < 0) {
        return true;
    }
    
    transport->socket_fd = -1;
    transport->state = MOCK_UDP_STATE_CLOSED;
    transport->bound = false;
    
    return true;
}

size_t mock_udp_transport_write(mock_udp_transport_t *transport, const uint8_t *buf, size_t len, uint8_t *err) {
    if (transport == NULL || buf == NULL) {
        if (err) *err = 1;
        return 0;
    }
    
    if (transport->socket_fd < 0) {
        if (err) *err = 1;
        return 0;
    }
    
    if (transport->state != MOCK_UDP_STATE_OPEN) {
        if (err) *err = 1;
        return 0;
    }
    
    if (len == 0) {
        if (err) *err = 0;
        return 0;
    }
    
    if (len > MOCK_MAX_UDP_PACKET_SIZE) {
        if (err) *err = 1;
        return 0;
    }
    
    if (error_config.sendto_fail) {
        if (err) *err = 1;
        return 0;
    }
    
    int result = error_config.next_sendto_result;
    if (result != 0) {
        error_config.next_sendto_result = 0;
        if (result < 0) {
            if (err) *err = 1;
            return 0;
        }
        transport->bytes_sent += (uint32_t)result;
        transport->write_count++;
        if (err) *err = 0;
        return (size_t)result;
    }
    
    transport->bytes_sent += (uint32_t)len;
    transport->write_count++;
    if (err) *err = 0;
    
    return len;
}

size_t mock_udp_transport_read(mock_udp_transport_t *transport, uint8_t *buf, size_t len, int timeout, uint8_t *err) {
    if (transport == NULL || buf == NULL) {
        if (err) *err = 1;
        return 0;
    }
    
    if (transport->socket_fd < 0) {
        if (err) *err = 1;
        return 0;
    }
    
    if (transport->state != MOCK_UDP_STATE_OPEN) {
        if (err) *err = 1;
        return 0;
    }
    
    if (timeout < 0) {
        if (err) *err = 1;
        return 0;
    }
    
    if (error_config.setsockopt_fail) {
        if (err) *err = 1;
        return 0;
    }
    
    if (error_config.next_recv_timeout_ms > 0) {
        error_config.next_recv_timeout_ms = 0;
        if (err) *err = 1;
        return 0;
    }
    
    if (error_config.recv_fail) {
        if (err) *err = 1;
        return 0;
    }
    
    int result = error_config.next_recv_result;
    if (result != 0) {
        error_config.next_recv_result = 0;
        if (result < 0) {
            if (err) *err = 1;
            return 0;
        }
        size_t bytes_to_copy = (size_t)result;
        if (bytes_to_copy > len) {
            bytes_to_copy = len;
        }
        memset(buf, 0xAB, bytes_to_copy);
        transport->bytes_received += (uint32_t)bytes_to_copy;
        transport->read_count++;
        if (err) *err = 0;
        return bytes_to_copy;
    }
    
    size_t bytes_to_read = (len < MOCK_MAX_UDP_PACKET_SIZE) ? len : MOCK_MAX_UDP_PACKET_SIZE;
    memset(buf, 0xAB, bytes_to_read);
    transport->bytes_received += (uint32_t)bytes_to_read;
    transport->read_count++;
    if (err) *err = 0;
    
    return bytes_to_read;
}
