#include "unity.h"
#include <string.h>
#include "mock_ip_config.h"

static ip_config_t test_config;

void setUp(void)
{
    memset(&test_config, 0, sizeof(ip_config_t));
}

void tearDown(void)
{
}

void test_ip_address_octets_valid(void)
{
    TEST_ASSERT_TRUE(ip_config_is_valid_octet(0));
    TEST_ASSERT_TRUE(ip_config_is_valid_octet(127));
    TEST_ASSERT_TRUE(ip_config_is_valid_octet(255));
    TEST_ASSERT_FALSE(ip_config_is_valid_octet(256));
    TEST_ASSERT_FALSE(ip_config_is_valid_octet(-1));
}

void test_parse_valid_ip_address(void)
{
    const char *ip_str = "192.168.1.100";
    TEST_ASSERT_TRUE(ip_config_parse_ip(ip_str, &test_config.ip));
    
    TEST_ASSERT_EQUAL_UINT8(192, test_config.ip.octets[0]);
    TEST_ASSERT_EQUAL_UINT8(168, test_config.ip.octets[1]);
    TEST_ASSERT_EQUAL_UINT8(1, test_config.ip.octets[2]);
    TEST_ASSERT_EQUAL_UINT8(100, test_config.ip.octets[3]);
}

void test_parse_invalid_ip_address_too_many_octets(void)
{
    const char *ip_str = "192.168.1.100.50";
    TEST_ASSERT_FALSE(ip_config_parse_ip(ip_str, &test_config.ip));
}

void test_parse_invalid_ip_address_too_few_octets(void)
{
    const char *ip_str = "192.168.1";
    TEST_ASSERT_FALSE(ip_config_parse_ip(ip_str, &test_config.ip));
}

void test_parse_invalid_ip_address_out_of_range(void)
{
    const char *ip_str = "192.168.1.300";
    TEST_ASSERT_FALSE(ip_config_parse_ip(ip_str, &test_config.ip));
}

void test_subnet_mask_validation(void)
{
    TEST_ASSERT_TRUE(ip_config_is_valid_subnet_mask(8));
    TEST_ASSERT_TRUE(ip_config_is_valid_subnet_mask(16));
    TEST_ASSERT_TRUE(ip_config_is_valid_subnet_mask(24));
    TEST_ASSERT_TRUE(ip_config_is_valid_subnet_mask(32));
    TEST_ASSERT_FALSE(ip_config_is_valid_subnet_mask(0));
    TEST_ASSERT_FALSE(ip_config_is_valid_subnet_mask(33));
    TEST_ASSERT_FALSE(ip_config_is_valid_subnet_mask(-1));
}

void test_subnet_mask_to_address(void)
{
    ip_address_t mask;
    
    ip_config_cidr_to_mask(24, &mask);
    TEST_ASSERT_EQUAL_UINT8(255, mask.octets[0]);
    TEST_ASSERT_EQUAL_UINT8(255, mask.octets[1]);
    TEST_ASSERT_EQUAL_UINT8(255, mask.octets[2]);
    TEST_ASSERT_EQUAL_UINT8(0, mask.octets[3]);
    
    ip_config_cidr_to_mask(16, &mask);
    TEST_ASSERT_EQUAL_UINT8(255, mask.octets[0]);
    TEST_ASSERT_EQUAL_UINT8(255, mask.octets[1]);
    TEST_ASSERT_EQUAL_UINT8(0, mask.octets[2]);
    TEST_ASSERT_EQUAL_UINT8(0, mask.octets[3]);
}

void test_network_address_detection(void)
{
    ip_address_t ip = {{192, 168, 1, 0}};
    ip_address_t mask = {{255, 255, 255, 0}};
    
    TEST_ASSERT_TRUE(ip_config_is_network_address(&ip, &mask));
    
    ip.octets[3] = 1;
    TEST_ASSERT_FALSE(ip_config_is_network_address(&ip, &mask));
}

void test_broadcast_address_detection(void)
{
    ip_address_t ip = {{192, 168, 1, 255}};
    ip_address_t mask = {{255, 255, 255, 0}};
    
    TEST_ASSERT_TRUE(ip_config_is_broadcast_address(&ip, &mask));
    
    ip.octets[3] = 254;
    TEST_ASSERT_FALSE(ip_config_is_broadcast_address(&ip, &mask));
}

void test_get_network_address(void)
{
    ip_address_t ip = {{192, 168, 1, 100}};
    ip_address_t mask = {{255, 255, 255, 0}};
    ip_address_t network;
    
    ip_config_get_network_address(&ip, &mask, &network);
    
    TEST_ASSERT_EQUAL_UINT8(192, network.octets[0]);
    TEST_ASSERT_EQUAL_UINT8(168, network.octets[1]);
    TEST_ASSERT_EQUAL_UINT8(1, network.octets[2]);
    TEST_ASSERT_EQUAL_UINT8(0, network.octets[3]);
}

void test_get_broadcast_address(void)
{
    ip_address_t ip = {{192, 168, 1, 100}};
    ip_address_t mask = {{255, 255, 255, 0}};
    ip_address_t broadcast;
    
    ip_config_get_broadcast_address(&ip, &mask, &broadcast);
    
    TEST_ASSERT_EQUAL_UINT8(192, broadcast.octets[0]);
    TEST_ASSERT_EQUAL_UINT8(168, broadcast.octets[1]);
    TEST_ASSERT_EQUAL_UINT8(1, broadcast.octets[2]);
    TEST_ASSERT_EQUAL_UINT8(255, broadcast.octets[3]);
}

void test_is_same_subnet(void)
{
    ip_address_t ip1 = {{192, 168, 1, 100}};
    ip_address_t ip2 = {{192, 168, 1, 200}};
    ip_address_t ip3 = {{192, 168, 2, 100}};
    ip_address_t mask = {{255, 255, 255, 0}};
    
    TEST_ASSERT_TRUE(ip_config_is_same_subnet(&ip1, &ip2, &mask));
    TEST_ASSERT_FALSE(ip_config_is_same_subnet(&ip1, &ip3, &mask));
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_ip_address_octets_valid);
    RUN_TEST(test_parse_valid_ip_address);
    RUN_TEST(test_parse_invalid_ip_address_too_many_octets);
    RUN_TEST(test_parse_invalid_ip_address_too_few_octets);
    RUN_TEST(test_parse_invalid_ip_address_out_of_range);
    RUN_TEST(test_subnet_mask_validation);
    RUN_TEST(test_subnet_mask_to_address);
    RUN_TEST(test_network_address_detection);
    RUN_TEST(test_broadcast_address_detection);
    RUN_TEST(test_get_network_address);
    RUN_TEST(test_get_broadcast_address);
    RUN_TEST(test_is_same_subnet);
    
    return UNITY_END();
}
