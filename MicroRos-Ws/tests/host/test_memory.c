#include "unity.h"
#include <string.h>
#include "mock_memory.h"

static void *allocated_ptr;

void setUp(void)
{
    allocated_ptr = NULL;
}

void tearDown(void)
{
    if (allocated_ptr != NULL)
    {
        mock_memory_deallocate(allocated_ptr);
        allocated_ptr = NULL;
    }
}

void test_allocate_returns_valid_pointer(void)
{
    allocated_ptr = mock_memory_allocate(128);
    TEST_ASSERT_NOT_NULL(allocated_ptr);
}

void test_allocate_zero_size_returns_null(void)
{
    void *ptr = mock_memory_allocate(0);
    TEST_ASSERT_NULL(ptr);
}

void test_deallocate_handles_null(void)
{
    mock_memory_deallocate(NULL);
    TEST_PASS();
}

void test_deallocate_valid_pointer_succeeds(void)
{
    void *ptr = mock_memory_allocate(64);
    TEST_ASSERT_NOT_NULL(ptr);
    mock_memory_deallocate(ptr);
    TEST_PASS();
}

void test_zero_allocate_zeros_memory(void)
{
    allocated_ptr = mock_memory_zero_allocate(16);
    TEST_ASSERT_NOT_NULL(allocated_ptr);
    
    unsigned char *bytes = (unsigned char *)allocated_ptr;
    for (int i = 0; i < 16; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0, bytes[i]);
    }
}

void test_reallocate_preserves_data(void)
{
    const char *test_data = "hello";
    size_t initial_size = 6;
    size_t new_size = 32;
    
    allocated_ptr = mock_memory_allocate(initial_size);
    TEST_ASSERT_NOT_NULL(allocated_ptr);
    
    memcpy(allocated_ptr, test_data, initial_size);
    
    void *new_ptr = mock_memory_reallocate(allocated_ptr, new_size);
    TEST_ASSERT_NOT_NULL(new_ptr);
    allocated_ptr = new_ptr;
    
    TEST_ASSERT_EQUAL_STRING(test_data, (char *)allocated_ptr);
}

void test_reallocate_null_behaves_like_allocate(void)
{
    void *ptr = mock_memory_reallocate(NULL, 64);
    TEST_ASSERT_NOT_NULL(ptr);
    mock_memory_deallocate(ptr);
}

void test_reallocate_to_zero_deallocates(void)
{
    void *ptr = mock_memory_allocate(64);
    TEST_ASSERT_NOT_NULL(ptr);
    
    void *result = mock_memory_reallocate(ptr, 0);
    TEST_ASSERT_NULL(result);
}

void test_multiple_allocations_succeed(void)
{
    void *ptr1 = mock_memory_allocate(32);
    void *ptr2 = mock_memory_allocate(64);
    void *ptr3 = mock_memory_allocate(128);
    
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NOT_NULL(ptr3);
    TEST_ASSERT_NOT_EQUAL(ptr1, ptr2);
    TEST_ASSERT_NOT_EQUAL(ptr2, ptr3);
    TEST_ASSERT_NOT_EQUAL(ptr1, ptr3);
    
    mock_memory_deallocate(ptr1);
    mock_memory_deallocate(ptr2);
    mock_memory_deallocate(ptr3);
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_allocate_returns_valid_pointer);
    RUN_TEST(test_allocate_zero_size_returns_null);
    RUN_TEST(test_deallocate_handles_null);
    RUN_TEST(test_deallocate_valid_pointer_succeeds);
    RUN_TEST(test_zero_allocate_zeros_memory);
    RUN_TEST(test_reallocate_preserves_data);
    RUN_TEST(test_reallocate_null_behaves_like_allocate);
    RUN_TEST(test_reallocate_to_zero_deallocates);
    RUN_TEST(test_multiple_allocations_succeed);
    
    return UNITY_END();
}
