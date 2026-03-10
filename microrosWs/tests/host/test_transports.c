#include "unity.h"
#include <string.h>
#include "mock_transports.h"

static transport_t test_transport;

void setUp(void)
{
    memset(&test_transport, 0, sizeof(transport_t));
    test_transport.state = TRANSPORT_STATE_CLOSED;
    mock_transports_reset();
}

void tearDown(void)
{
    if (test_transport.state == TRANSPORT_STATE_OPEN)
    {
        transport_close(&test_transport);
    }
}

void test_transport_initialized_state_is_closed(void)
{
    transport_t transport = {0};
    TEST_ASSERT_EQUAL(TRANSPORT_STATE_CLOSED, transport.state);
}

void test_transport_open_transitions_to_open(void)
{
    transport_result_t result = transport_open(&test_transport);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_OK, result);
    TEST_ASSERT_EQUAL(TRANSPORT_STATE_OPEN, test_transport.state);
    TEST_ASSERT_TRUE(test_transport.connected);
}

void test_transport_close_transitions_to_closed(void)
{
    transport_open(&test_transport);
    transport_result_t result = transport_close(&test_transport);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_OK, result);
    TEST_ASSERT_EQUAL(TRANSPORT_STATE_CLOSED, test_transport.state);
    TEST_ASSERT_FALSE(test_transport.connected);
}

void test_transport_close_when_already_closed(void)
{
    transport_result_t result = transport_close(&test_transport);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_OK, result);
    TEST_ASSERT_EQUAL(TRANSPORT_STATE_CLOSED, test_transport.state);
}

void test_transport_open_with_null_fails(void)
{
    transport_result_t result = transport_open(NULL);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_INVALID_PARAM, result);
}

void test_transport_close_with_null_fails(void)
{
    transport_result_t result = transport_close(NULL);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_INVALID_PARAM, result);
}

void test_transport_open_fails_when_network_down(void)
{
    mock_transports_set_network_up(false);
    transport_result_t result = transport_open(&test_transport);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_ERROR, result);
    TEST_ASSERT_EQUAL(TRANSPORT_STATE_ERROR, test_transport.state);
}

void test_transport_open_fails_when_link_down(void)
{
    mock_transports_set_link_up(false);
    transport_result_t result = transport_open(&test_transport);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_ERROR, result);
    TEST_ASSERT_EQUAL(TRANSPORT_STATE_ERROR, test_transport.state);
}

void test_transport_write_succeeds_when_open(void)
{
    transport_open(&test_transport);
    uint8_t data[] = {0x01, 0x02, 0x03};
    transport_result_t result = transport_write(&test_transport, data, sizeof(data));
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_OK, result);
    TEST_ASSERT_EQUAL_UINT32(3, test_transport.bytes_sent);
}

void test_transport_write_fails_when_closed(void)
{
    uint8_t data[] = {0x01, 0x02, 0x03};
    transport_result_t result = transport_write(&test_transport, data, sizeof(data));
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_ERROR, result);
}

void test_transport_write_with_null_fails(void)
{
    transport_open(&test_transport);
    uint8_t data[] = {0x01, 0x02, 0x03};
    transport_result_t result = transport_write(NULL, data, sizeof(data));
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_INVALID_PARAM, result);
    
    result = transport_write(&test_transport, NULL, sizeof(data));
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_INVALID_PARAM, result);
}

void test_transport_read_succeeds_when_open(void)
{
    transport_open(&test_transport);
    uint8_t buffer[32];
    size_t bytes_read = 0;
    
    transport_result_t result = transport_read(&test_transport, buffer, sizeof(buffer), &bytes_read);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_OK, result);
    TEST_ASSERT_EQUAL_UINT32(sizeof(buffer), bytes_read);
    TEST_ASSERT_EQUAL_UINT32(sizeof(buffer), test_transport.bytes_received);
}

void test_transport_read_timeout(void)
{
    transport_open(&test_transport);
    mock_transports_set_next_read_timeout(true);
    
    uint8_t buffer[32];
    size_t bytes_read = 0;
    
    transport_result_t result = transport_read(&test_transport, buffer, sizeof(buffer), &bytes_read);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_TIMEOUT, result);
    TEST_ASSERT_EQUAL_UINT32(0, bytes_read);
}

void test_transport_read_fails_when_closed(void)
{
    uint8_t buffer[32];
    size_t bytes_read = 0;
    
    transport_result_t result = transport_read(&test_transport, buffer, sizeof(buffer), &bytes_read);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_ERROR, result);
}

void test_transport_read_with_null_fails(void)
{
    transport_open(&test_transport);
    uint8_t buffer[32];
    size_t bytes_read = 0;
    
    transport_result_t result = transport_read(NULL, buffer, sizeof(buffer), &bytes_read);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_INVALID_PARAM, result);
    
    result = transport_read(&test_transport, NULL, sizeof(buffer), &bytes_read);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_INVALID_PARAM, result);
    
    result = transport_read(&test_transport, buffer, sizeof(buffer), NULL);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_INVALID_PARAM, result);
}

void test_transport_network_status_functions(void)
{
    TEST_ASSERT_TRUE(transport_is_network_up());
    TEST_ASSERT_TRUE(transport_is_link_up());
    
    mock_transports_set_network_up(false);
    TEST_ASSERT_FALSE(transport_is_network_up());
    
    mock_transports_set_link_up(false);
    TEST_ASSERT_FALSE(transport_is_link_up());
}

void test_transport_error_injection(void)
{
    mock_transports_set_next_error(true);
    transport_result_t result = transport_open(&test_transport);
    TEST_ASSERT_EQUAL(TRANSPORT_RESULT_ERROR, result);
    TEST_ASSERT_EQUAL_UINT8(1, test_transport.error_count);
}

void test_transport_multiple_operations(void)
{
    transport_open(&test_transport);
    
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    transport_write(&test_transport, data, sizeof(data));
    transport_write(&test_transport, data, sizeof(data));
    
    TEST_ASSERT_EQUAL_UINT32(sizeof(data) * 2, test_transport.bytes_sent);
    
    uint8_t buffer[10];
    size_t bytes_read;
    transport_read(&test_transport, buffer, sizeof(buffer), &bytes_read);
    
    TEST_ASSERT_EQUAL_UINT32(sizeof(buffer), test_transport.bytes_received);
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_transport_initialized_state_is_closed);
    RUN_TEST(test_transport_open_transitions_to_open);
    RUN_TEST(test_transport_close_transitions_to_closed);
    RUN_TEST(test_transport_close_when_already_closed);
    RUN_TEST(test_transport_open_with_null_fails);
    RUN_TEST(test_transport_open_fails_when_network_down);
    RUN_TEST(test_transport_open_fails_when_link_down);
    RUN_TEST(test_transport_write_succeeds_when_open);
    RUN_TEST(test_transport_write_fails_when_closed);
    RUN_TEST(test_transport_write_with_null_fails);
    RUN_TEST(test_transport_read_succeeds_when_open);
    RUN_TEST(test_transport_read_timeout);
    RUN_TEST(test_transport_read_fails_when_closed);
    RUN_TEST(test_transport_read_with_null_fails);
    RUN_TEST(test_transport_network_status_functions);
    RUN_TEST(test_transport_error_injection);
    RUN_TEST(test_transport_multiple_operations);
    
    return UNITY_END();
}
