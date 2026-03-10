#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "../host/mocks/mock_udp_transport.h"

#define MAX_IP_ADDR_LEN 16
#define MAX_PACKET_SIZE 65535

static mock_udp_transport_t fuzz_transport;
static bool fuzz_initialized = false;

typedef struct {
    uint8_t op_type;
    uint8_t data_len;
    uint16_t timeout;
    uint32_t buffer_size;
    char ip_addr[16];
} fuzz_operation_header_t;

typedef enum {
    FUZZ_OP_OPEN = 0x01,
    FUZZ_OP_CLOSE = 0x02,
    FUZZ_OP_WRITE = 0x03,
    FUZZ_OP_READ = 0x04,
    FUZZ_OP_WRITE_READ = 0x05,
    FUZZ_OP_ERROR_INJECT = 0x06,
    FUZZ_OP_RESET = 0x07,
    FUZZ_OP_STRESS = 0x08
} fuzz_operation_type_t;

static void fuzz_udp_transport_init(void) {
    if (!fuzz_initialized) {
        memset(&fuzz_transport, 0, sizeof(mock_udp_transport_t));
        mock_udp_transport_reset();
        fuzz_initialized = true;
    }
}

static void fuzz_transport_reset(void) {
    if (fuzz_transport.state == MOCK_UDP_STATE_OPEN) {
        mock_udp_transport_close(&fuzz_transport);
    }
    mock_udp_transport_reset();
    memset(&fuzz_transport, 0, sizeof(mock_udp_transport_t));
    fuzz_initialized = false;
    fuzz_udp_transport_init();
}

static const char* generate_ip_from_fuzz(const uint8_t *data, size_t len) {
    static char ip_buf[MAX_IP_ADDR_LEN];
    
    if (len >= 4) {
        snprintf(ip_buf, sizeof(ip_buf), "%d.%d.%d.%d",
                 (data[0] % 256), (data[1] % 256),
                 (data[2] % 256), (data[3] % 256));
    } else {
        strncpy(ip_buf, "192.168.1.100", sizeof(ip_buf) - 1);
        ip_buf[sizeof(ip_buf) - 1] = '\0';
    }
    
    return ip_buf;
}

static void inject_error_from_byte(uint8_t error_byte) {
    mock_udp_transport_set_socket_create_fail((error_byte & 0x01) != 0);
    mock_udp_transport_set_bind_fail((error_byte & 0x02) != 0);
    mock_udp_transport_set_sendto_fail((error_byte & 0x04) != 0);
    mock_udp_transport_set_recv_fail((error_byte & 0x08) != 0);
    mock_udp_transport_set_setsockopt_fail((error_byte & 0x10) != 0);
    
    if (error_byte & 0x20) {
        mock_udp_transport_set_next_recv_timeout((error_byte & 0x0F) * 100);
    }
    if (error_byte & 0x40) {
        mock_udp_transport_set_next_sendto_result((int8_t)(error_byte));
    }
    if (error_byte & 0x80) {
        mock_udp_transport_set_next_recv_result((int8_t)(error_byte));
    }
}

static void fuzz_test_transport_open(const uint8_t *data, size_t len) {
    const char *ip = generate_ip_from_fuzz(data, len);
    bool result = mock_udp_transport_open(&fuzz_transport, ip);
    
    (void)result;
    
    if (fuzz_transport.state == MOCK_UDP_STATE_OPEN) {
        if (!mock_udp_transport_close(&fuzz_transport)) {
        }
    }
}

static void fuzz_test_transport_write(const uint8_t *data, size_t len) {
    if (len == 0) return;
    
    mock_udp_transport_open(&fuzz_transport, "192.168.1.100");
    
    uint8_t err = 0;
    size_t written;
    
    if (data[0] == 0xFF && len > 1) {
        size_t offset = 1;
        while (offset < len) {
            size_t chunk_size = data[offset] % 64;
            if (offset + 1 + chunk_size > len) break;
            
            written = mock_udp_transport_write(&fuzz_transport,
                                               data + offset + 1,
                                               chunk_size, &err);
            offset += 1 + chunk_size;
            (void)written;
        }
    } else {
        written = mock_udp_transport_write(&fuzz_transport, data, len, &err);
        (void)written;
    }
    
    mock_udp_transport_close(&fuzz_transport);
}

static void fuzz_test_transport_read(const uint8_t *data, size_t len) {
    mock_udp_transport_open(&fuzz_transport, "192.168.1.100");
    
    uint8_t buffer[MOCK_MAX_UDP_PACKET_SIZE * 2];
    uint8_t err = 0;
    int timeout = 1000;
    
    if (len >= 2) {
        timeout = (data[0] << 8) | data[1];
        if (timeout < 0) timeout = 0;
        if (timeout > 10000) timeout = 10000;
    }
    
    size_t buffer_size = sizeof(buffer);
    if (len >= 6) {
        buffer_size = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
        if (buffer_size > sizeof(buffer)) buffer_size = sizeof(buffer);
        if (buffer_size == 0) buffer_size = 1;
    }
    
    if (len > 6 && data[6] == 0xAA) {
        for (int i = 0; i < (data[7] % 10 + 1); i++) {
            size_t bytes_read = mock_udp_transport_read(&fuzz_transport,
                                                        buffer, buffer_size,
                                                        timeout, &err);
            (void)bytes_read;
        }
    } else {
        size_t bytes_read = mock_udp_transport_read(&fuzz_transport,
                                                    buffer, buffer_size,
                                                    timeout, &err);
        (void)bytes_read;
    }
    
    mock_udp_transport_close(&fuzz_transport);
}

static void fuzz_test_write_read_cycle(const uint8_t *data, size_t len) {
    mock_udp_transport_open(&fuzz_transport, "192.168.1.100");
    
    uint8_t write_buf[MOCK_MAX_UDP_PACKET_SIZE];
    uint8_t read_buf[MOCK_MAX_UDP_PACKET_SIZE];
    uint8_t err = 0;
    
    size_t half = len / 2;
    if (half == 0) half = 1;
    if (half > sizeof(write_buf)) half = sizeof(write_buf);
    
    memcpy(write_buf, data, half);
    
    size_t written = mock_udp_transport_write(&fuzz_transport, write_buf, half, &err);
    (void)written;
    
    size_t bytes_read = mock_udp_transport_read(&fuzz_transport, read_buf,
                                                sizeof(read_buf), 100, &err);
    (void)bytes_read;
    
    mock_udp_transport_close(&fuzz_transport);
}

static void fuzz_test_error_injection(const uint8_t *data, size_t len) {
    if (len < 2) return;
    
    for (size_t i = 0; i < len; i++) {
        mock_udp_transport_reset();
        inject_error_from_byte(data[i]);
        
        bool open_result = mock_udp_transport_open(&fuzz_transport, "192.168.1.100");
        (void)open_result;
        
        if (fuzz_transport.state == MOCK_UDP_STATE_OPEN) {
            uint8_t buf[64];
            uint8_t err = 0;
            
            mock_udp_transport_write(&fuzz_transport, data + i, 
                                    (len - i < 64) ? len - i : 64, &err);
            mock_udp_transport_read(&fuzz_transport, buf, sizeof(buf), 100, &err);
        }
        
        mock_udp_transport_close(&fuzz_transport);
    }
}

static void fuzz_test_stress(const uint8_t *data, size_t len) {
    if (len == 0) return;
    
    uint8_t buffer[256];
    uint8_t err = 0;
    int iterations = data[0] % 50 + 1;
    
    for (int i = 0; i < iterations; i++) {
        size_t idx = (i % (int)len);
        
        switch (data[idx] % 5) {
            case 0:
                mock_udp_transport_open(&fuzz_transport, 
                                       generate_ip_from_fuzz(data + idx, len - idx));
                break;
            case 1:
                mock_udp_transport_close(&fuzz_transport);
                break;
            case 2:
                if (fuzz_transport.state == MOCK_UDP_STATE_OPEN) {
                    mock_udp_transport_write(&fuzz_transport, data + idx,
                                            (len - idx < 128) ? len - idx : 128, &err);
                }
                break;
            case 3:
                if (fuzz_transport.state == MOCK_UDP_STATE_OPEN) {
                    mock_udp_transport_read(&fuzz_transport, buffer,
                                           sizeof(buffer), 50, &err);
                }
                break;
            case 4:
                inject_error_from_byte(data[idx]);
                break;
        }
    }
    
    mock_udp_transport_close(&fuzz_transport);
}

static void fuzz_boundary_conditions(void) {
    uint8_t err = 0;
    uint8_t buffer[1];
    uint8_t large_buffer[MOCK_MAX_UDP_PACKET_SIZE + 1];
    
    fuzz_transport_reset();
    
    bool result = mock_udp_transport_open(NULL, "192.168.1.100");
    (void)result;
    
    result = mock_udp_transport_open(&fuzz_transport, NULL);
    (void)result;
    
    result = mock_udp_transport_open(&fuzz_transport, "");
    (void)result;
    
    result = mock_udp_transport_open(&fuzz_transport, "999.999.999.999");
    (void)result;
    
    result = mock_udp_transport_open(&fuzz_transport, "192.168.1.100");
    if (result) {
        size_t written = mock_udp_transport_write(&fuzz_transport, NULL, 10, &err);
        (void)written;
        
        written = mock_udp_transport_write(&fuzz_transport, large_buffer, 0, &err);
        (void)written;
        
        written = mock_udp_transport_write(&fuzz_transport, large_buffer,
                                          MOCK_MAX_UDP_PACKET_SIZE + 1, &err);
        (void)written;
        
        size_t bytes_read = mock_udp_transport_read(&fuzz_transport, NULL, 10, 100, &err);
        (void)bytes_read;
        
        bytes_read = mock_udp_transport_read(&fuzz_transport, buffer, 0, 100, &err);
        (void)bytes_read;
        
        bytes_read = mock_udp_transport_read(&fuzz_transport, buffer, 1, -1, &err);
        (void)bytes_read;
        
        bytes_read = mock_udp_transport_read(NULL, buffer, 1, 100, &err);
        (void)bytes_read;
        
        mock_udp_transport_close(&fuzz_transport);
    }
    
    result = mock_udp_transport_close(NULL);
    (void)result;
    
    result = mock_udp_transport_close(&fuzz_transport);
    (void)result;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len) {
    fuzz_udp_transport_init();
    
    fuzz_boundary_conditions();
    
    if (len == 0) {
        fuzz_transport_reset();
        return 0;
    }
    
    uint8_t operation = data[0];
    const uint8_t *payload = data + 1;
    size_t payload_len = len - 1;
    
    fuzz_transport_reset();
    
    switch (operation) {
        case FUZZ_OP_OPEN:
            fuzz_test_transport_open(payload, payload_len);
            break;
        case FUZZ_OP_CLOSE:
            mock_udp_transport_open(&fuzz_transport, 
                                   generate_ip_from_fuzz(payload, payload_len));
            mock_udp_transport_close(&fuzz_transport);
            break;
        case FUZZ_OP_WRITE:
            fuzz_test_transport_write(payload, payload_len);
            break;
        case FUZZ_OP_READ:
            fuzz_test_transport_read(payload, payload_len);
            break;
        case FUZZ_OP_WRITE_READ:
            fuzz_test_write_read_cycle(payload, payload_len);
            break;
        case FUZZ_OP_ERROR_INJECT:
            fuzz_test_error_injection(payload, payload_len);
            break;
        case FUZZ_OP_RESET:
            fuzz_transport_reset();
            mock_udp_transport_open(&fuzz_transport,
                                   generate_ip_from_fuzz(payload, payload_len));
            mock_udp_transport_close(&fuzz_transport);
            break;
        case FUZZ_OP_STRESS:
            fuzz_test_stress(payload, payload_len);
            break;
        default: {
            fuzz_test_transport_open(data, len);
            fuzz_test_transport_write(data, len);
            fuzz_test_transport_read(data, len);
            fuzz_test_write_read_cycle(data, len);
            break;
        }
    }
    
    fuzz_transport_reset();
    
    return 0;
}