#include "unity.h"
#include <string.h>
#include "mock_udp_transport.h"

static mock_udp_transport_t test_transport;

void setUp(void)
{
    memset(&test_transport, 0, sizeof(mock_udp_transport_t));
    mock_udp_transport_reset();
}

void tearDown(void)
{
    if (test_transport.state == MOCK_UDP_STATE_OPEN)
    {
        mock_udp_transport_close(&test_transport);
    }
}

void test_udp_transport_initial_state_is_closed(void)
{
    mock_udp_transport_t *instance = mock_udp_transport_get_instance();
    TEST_ASSERT_EQUAL(MOCK_UDP_STATE_CLOSED, instance->state);
    TEST_ASSERT_EQUAL(-1, instance->socket_fd);
}

void test_udp_transport_open_success(void)
{
    bool result = mock_udp_transport_open(&test_transport, "192.168.1.100");
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(MOCK_UDP_STATE_OPEN, test_transport.state);
    TEST_ASSERT_TRUE(test_transport.bound);
    TEST_ASSERT_EQUAL(42, test_transport.socket_fd);
}

void test_udp_transport_open_with_null_transport_fails(void)
{
    bool result = mock_udp_transport_open(NULL, "192.168.1.100");
    TEST_ASSERT_FALSE(result);
}

void test_udp_transport_open_socket_creation_failure(void)
{
    mock_udp_transport_set_socket_create_fail(true);
    bool result = mock_udp_transport_open(&test_transport, "192.168.1.100");
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(MOCK_UDP_STATE_ERROR, test_transport.state);
    TEST_ASSERT_EQUAL(1, test_transport.last_error);
}

void test_udp_transport_open_bind_failure(void)
{
    mock_udp_transport_set_bind_fail(true);
    bool result = mock_udp_transport_open(&test_transport, "192.168.1.100");
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(MOCK_UDP_STATE_ERROR, test_transport.state);
    TEST_ASSERT_EQUAL(2, test_transport.last_error);
}

void test_udp_transport_close_success(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    bool result = mock_udp_transport_close(&test_transport);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(MOCK_UDP_STATE_CLOSED, test_transport.state);
    TEST_ASSERT_EQUAL(-1, test_transport.socket_fd);
    TEST_ASSERT_FALSE(test_transport.bound);
}

void test_udp_transport_close_when_already_closed(void)
{
    bool result = mock_udp_transport_close(&test_transport);
    TEST_ASSERT_TRUE(result);
}

void test_udp_transport_close_with_null_transport(void)
{
    bool result = mock_udp_transport_close(NULL);
    TEST_ASSERT_FALSE(result);
}

void test_udp_transport_write_success(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t err = 0;
    
    size_t written = mock_udp_transport_write(&test_transport, data, sizeof(data), &err);
    TEST_ASSERT_EQUAL(sizeof(data), written);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL_UINT32(sizeof(data), test_transport.bytes_sent);
    TEST_ASSERT_EQUAL_UINT32(1, test_transport.write_count);
}

void test_udp_transport_write_with_null_transport_fails(void)
{
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t err = 0;
    
    size_t written = mock_udp_transport_write(NULL, data, sizeof(data), &err);
    TEST_ASSERT_EQUAL(0, written);
    TEST_ASSERT_EQUAL(1, err);
}

void test_udp_transport_write_with_null_buffer_fails(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    uint8_t err = 0;
    
    size_t written = mock_udp_transport_write(&test_transport, NULL, 10, &err);
    TEST_ASSERT_EQUAL(0, written);
    TEST_ASSERT_EQUAL(1, err);
}

void test_udp_transport_write_when_closed_fails(void)
{
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t err = 0;
    
    size_t written = mock_udp_transport_write(&test_transport, data, sizeof(data), &err);
    TEST_ASSERT_EQUAL(0, written);
    TEST_ASSERT_EQUAL(1, err);
}

void test_udp_transport_write_zero_length(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    uint8_t data[] = {0x01};
    uint8_t err = 0;
    
    size_t written = mock_udp_transport_write(&test_transport, data, 0, &err);
    TEST_ASSERT_EQUAL(0, written);
    TEST_ASSERT_EQUAL(0, err);
}

void test_udp_transport_write_failure_injection(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    mock_udp_transport_set_sendto_fail(true);
    
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t err = 0;
    
    size_t written = mock_udp_transport_write(&test_transport, data, sizeof(data), &err);
    TEST_ASSERT_EQUAL(0, written);
    TEST_ASSERT_EQUAL(1, err);
}

void test_udp_transport_write_custom_result(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    mock_udp_transport_set_next_sendto_result(3);
    
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t err = 0;
    
    size_t written = mock_udp_transport_write(&test_transport, data, sizeof(data), &err);
    TEST_ASSERT_EQUAL(3, written);
    TEST_ASSERT_EQUAL(0, err);
}

void test_udp_transport_read_success(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    uint8_t buffer[32];
    uint8_t err = 0;
    
    size_t result = mock_udp_transport_read(&test_transport, buffer, sizeof(buffer), 1000, &err);
    TEST_ASSERT_TRUE(result > 0);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_TRUE(test_transport.bytes_received > 0);
    TEST_ASSERT_EQUAL_UINT32(1, test_transport.read_count);
}

void test_udp_transport_read_with_null_transport_fails(void)
{
    uint8_t buffer[32];
    uint8_t err = 0;
    
    size_t result = mock_udp_transport_read(NULL, buffer, sizeof(buffer), 1000, &err);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(1, err);
}

void test_udp_transport_read_with_null_buffer_fails(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    uint8_t err = 0;
    
    size_t result = mock_udp_transport_read(&test_transport, NULL, 32, 1000, &err);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(1, err);
}

void test_udp_transport_read_when_closed_fails(void)
{
    uint8_t buffer[32];
    uint8_t err = 0;
    
    size_t result = mock_udp_transport_read(&test_transport, buffer, sizeof(buffer), 1000, &err);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(1, err);
}

void test_udp_transport_read_timeout_scenario(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    mock_udp_transport_set_next_recv_timeout(1000);
    
    uint8_t buffer[32];
    uint8_t err = 0;
    
    size_t result = mock_udp_transport_read(&test_transport, buffer, sizeof(buffer), 1000, &err);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(1, err);
}

void test_udp_transport_read_failure_injection(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    mock_udp_transport_set_recv_fail(true);
    
    uint8_t buffer[32];
    uint8_t err = 0;
    
    size_t result = mock_udp_transport_read(&test_transport, buffer, sizeof(buffer), 1000, &err);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(1, err);
}

void test_udp_transport_read_setsockopt_failure(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    mock_udp_transport_set_setsockopt_fail(true);
    
    uint8_t buffer[32];
    uint8_t err = 0;
    
    size_t result = mock_udp_transport_read(&test_transport, buffer, sizeof(buffer), 1000, &err);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(1, err);
}

void test_udp_transport_read_custom_result(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    mock_udp_transport_set_next_recv_result(10);
    
    uint8_t buffer[32];
    uint8_t err = 0;
    
    size_t result = mock_udp_transport_read(&test_transport, buffer, sizeof(buffer), 1000, &err);
    TEST_ASSERT_EQUAL(10, result);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL_UINT32(10, test_transport.bytes_received);
}

void test_udp_transport_read_negative_timeout_fails(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    
    uint8_t buffer[32];
    uint8_t err = 0;
    
    size_t result = mock_udp_transport_read(&test_transport, buffer, sizeof(buffer), -1, &err);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(1, err);
}

void test_udp_transport_multiple_operations(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t err = 0;
    
    mock_udp_transport_write(&test_transport, data, sizeof(data), &err);
    mock_udp_transport_write(&test_transport, data, sizeof(data), &err);
    mock_udp_transport_write(&test_transport, data, sizeof(data), &err);
    
    TEST_ASSERT_EQUAL_UINT32(3, test_transport.write_count);
    TEST_ASSERT_EQUAL_UINT32(sizeof(data) * 3, test_transport.bytes_sent);
    
    uint8_t buffer[32];
    mock_udp_transport_read(&test_transport, buffer, sizeof(buffer), 1000, &err);
    mock_udp_transport_read(&test_transport, buffer, sizeof(buffer), 1000, &err);
    
    TEST_ASSERT_EQUAL_UINT32(2, test_transport.read_count);
    
    mock_udp_transport_close(&test_transport);
    TEST_ASSERT_EQUAL(MOCK_UDP_STATE_CLOSED, test_transport.state);
}

void test_udp_transport_instance_access(void)
{
    mock_udp_transport_t *instance = mock_udp_transport_get_instance();
    TEST_ASSERT_NOT_NULL(instance);
    TEST_ASSERT_EQUAL(MOCK_UDP_STATE_CLOSED, instance->state);
}

void test_udp_transport_reset_clears_state(void)
{
    mock_udp_transport_open(&test_transport, "192.168.1.100");
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t err = 0;
    mock_udp_transport_write(&test_transport, data, sizeof(data), &err);
    
    mock_udp_transport_reset();
    
    mock_udp_transport_t *instance = mock_udp_transport_get_instance();
    TEST_ASSERT_EQUAL(MOCK_UDP_STATE_CLOSED, instance->state);
    TEST_ASSERT_EQUAL(-1, instance->socket_fd);
    TEST_ASSERT_EQUAL_UINT32(0, instance->bytes_sent);
    TEST_ASSERT_EQUAL_UINT32(0, instance->bytes_received);
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_udp_transport_initial_state_is_closed);
    RUN_TEST(test_udp_transport_open_success);
    RUN_TEST(test_udp_transport_open_with_null_transport_fails);
    RUN_TEST(test_udp_transport_open_socket_creation_failure);
    RUN_TEST(test_udp_transport_open_bind_failure);
    RUN_TEST(test_udp_transport_close_success);
    RUN_TEST(test_udp_transport_close_when_already_closed);
    RUN_TEST(test_udp_transport_close_with_null_transport);
    RUN_TEST(test_udp_transport_write_success);
    RUN_TEST(test_udp_transport_write_with_null_transport_fails);
    RUN_TEST(test_udp_transport_write_with_null_buffer_fails);
    RUN_TEST(test_udp_transport_write_when_closed_fails);
    RUN_TEST(test_udp_transport_write_zero_length);
    RUN_TEST(test_udp_transport_write_failure_injection);
    RUN_TEST(test_udp_transport_write_custom_result);
    RUN_TEST(test_udp_transport_read_success);
    RUN_TEST(test_udp_transport_read_with_null_transport_fails);
    RUN_TEST(test_udp_transport_read_with_null_buffer_fails);
    RUN_TEST(test_udp_transport_read_when_closed_fails);
    RUN_TEST(test_udp_transport_read_timeout_scenario);
    RUN_TEST(test_udp_transport_read_failure_injection);
    RUN_TEST(test_udp_transport_read_setsockopt_failure);
    RUN_TEST(test_udp_transport_read_custom_result);
    RUN_TEST(test_udp_transport_read_negative_timeout_fails);
    RUN_TEST(test_udp_transport_multiple_operations);
    RUN_TEST(test_udp_transport_instance_access);
    RUN_TEST(test_udp_transport_reset_clears_state);
    
    return UNITY_END();
}
