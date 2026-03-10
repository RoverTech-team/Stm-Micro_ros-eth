#ifndef MOCK_TRANSPORTS_H
#define MOCK_TRANSPORTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Transport states */
typedef enum {
    TRANSPORT_STATE_CLOSED = 0,
    TRANSPORT_STATE_OPEN,
    TRANSPORT_STATE_ERROR
} transport_state_t;

/* Transport result codes */
typedef enum {
    TRANSPORT_RESULT_OK = 0,
    TRANSPORT_RESULT_ERROR,
    TRANSPORT_RESULT_TIMEOUT,
    TRANSPORT_RESULT_INVALID_PARAM
} transport_result_t;

/* Transport structure */
typedef struct {
    transport_state_t state;
    uint8_t error_count;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    bool connected;
} transport_t;

/* Transport functions */
transport_result_t transport_open(transport_t *transport);
transport_result_t transport_close(transport_t *transport);
transport_result_t transport_write(transport_t *transport, const uint8_t *data, size_t len);
transport_result_t transport_read(transport_t *transport, uint8_t *data, size_t len, size_t *bytes_read);

/* Network status functions */
bool transport_is_network_up(void);
bool transport_is_link_up(void);

/* Mock control functions */
void mock_transports_reset(void);
void mock_transports_set_network_up(bool up);
void mock_transports_set_link_up(bool up);
void mock_transports_set_next_read_timeout(bool timeout);
void mock_transports_set_next_error(bool error);

#endif /* MOCK_TRANSPORTS_H */
