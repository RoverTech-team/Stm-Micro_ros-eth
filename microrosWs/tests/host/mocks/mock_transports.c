#include "mock_transports.h"
#include <string.h>

/* Internal mock state */
static bool network_up = true;
static bool link_up = true;
static bool next_read_timeout = false;
static bool next_error = false;

void mock_transports_reset(void) {
    network_up = true;
    link_up = true;
    next_read_timeout = false;
    next_error = false;
}

void mock_transports_set_network_up(bool up) {
    network_up = up;
}

void mock_transports_set_link_up(bool up) {
    link_up = up;
}

void mock_transports_set_next_read_timeout(bool timeout) {
    next_read_timeout = timeout;
}

void mock_transports_set_next_error(bool error) {
    next_error = error;
}

transport_result_t transport_open(transport_t *transport) {
    if (transport == NULL) {
        return TRANSPORT_RESULT_INVALID_PARAM;
    }
    
    if (next_error) {
        next_error = false;
        transport->state = TRANSPORT_STATE_ERROR;
        transport->error_count++;
        return TRANSPORT_RESULT_ERROR;
    }
    
    if (!network_up || !link_up) {
        transport->state = TRANSPORT_STATE_ERROR;
        return TRANSPORT_RESULT_ERROR;
    }
    
    transport->state = TRANSPORT_STATE_OPEN;
    transport->connected = true;
    transport->bytes_sent = 0;
    transport->bytes_received = 0;
    transport->error_count = 0;
    
    return TRANSPORT_RESULT_OK;
}

transport_result_t transport_close(transport_t *transport) {
    if (transport == NULL) {
        return TRANSPORT_RESULT_INVALID_PARAM;
    }
    
    transport->state = TRANSPORT_STATE_CLOSED;
    transport->connected = false;
    
    return TRANSPORT_RESULT_OK;
}

transport_result_t transport_write(transport_t *transport, const uint8_t *data, size_t len) {
    if (transport == NULL || data == NULL) {
        return TRANSPORT_RESULT_INVALID_PARAM;
    }
    
    if (transport->state != TRANSPORT_STATE_OPEN) {
        return TRANSPORT_RESULT_ERROR;
    }
    
    if (next_error) {
        next_error = false;
        transport->error_count++;
        return TRANSPORT_RESULT_ERROR;
    }
    
    transport->bytes_sent += len;
    return TRANSPORT_RESULT_OK;
}

transport_result_t transport_read(transport_t *transport, uint8_t *data, size_t len, size_t *bytes_read) {
    if (transport == NULL || data == NULL || bytes_read == NULL) {
        return TRANSPORT_RESULT_INVALID_PARAM;
    }
    
    if (transport->state != TRANSPORT_STATE_OPEN) {
        return TRANSPORT_RESULT_ERROR;
    }
    
    if (next_read_timeout) {
        next_read_timeout = false;
        *bytes_read = 0;
        return TRANSPORT_RESULT_TIMEOUT;
    }
    
    if (next_error) {
        next_error = false;
        transport->error_count++;
        return TRANSPORT_RESULT_ERROR;
    }
    
    /* Simulate reading - fill with dummy data */
    memset(data, 0xAB, len);
    *bytes_read = len;
    transport->bytes_received += len;
    
    return TRANSPORT_RESULT_OK;
}

bool transport_is_network_up(void) {
    return network_up;
}

bool transport_is_link_up(void) {
    return link_up;
}
