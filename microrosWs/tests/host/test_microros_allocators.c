#include "unity.h"
#include <string.h>
#include "mock_microros_allocators.h"

void setUp(void)
{
    mock_microros_allocator_reset();
}

void tearDown(void)
{
    mock_microros_allocator_reset();
}

void test_allocate_returns_valid_pointer(void)
{
    void *ptr = mock_microros_allocate(128, NULL);
    TEST_ASSERT_NOT_NULL(ptr);
    mock_microros_deallocate(ptr, NULL);
}

void test_allocate_zero_size_returns_null(void)
{
    void *ptr = mock_microros_allocate(0, NULL);
    TEST_ASSERT_NULL(ptr);
}

void test_allocate_updates_stats(void)
{
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    
    void *ptr = mock_microros_allocate(256, NULL);
    TEST_ASSERT_NOT_NULL(ptr);
    
    TEST_ASSERT_EQUAL_UINT(1, stats->total_allocations);
    TEST_ASSERT_EQUAL_UINT(1, stats->current_allocated_blocks);
    TEST_ASSERT_EQUAL_UINT(256, stats->total_bytes_allocated);
    TEST_ASSERT_EQUAL_UINT(256, stats->current_bytes_in_use);
    TEST_ASSERT_EQUAL_UINT(256, stats->last_allocation_size);
    
    mock_microros_deallocate(ptr, NULL);
}

void test_deallocate_updates_stats(void)
{
    void *ptr = mock_microros_allocate(128, NULL);
    TEST_ASSERT_NOT_NULL(ptr);
    
    mock_microros_deallocate(ptr, NULL);
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(1, stats->total_deallocations);
    TEST_ASSERT_EQUAL_UINT(0, stats->current_allocated_blocks);
    TEST_ASSERT_EQUAL_UINT(128, stats->total_bytes_freed);
    TEST_ASSERT_EQUAL_UINT(0, stats->current_bytes_in_use);
}

void test_deallocate_null_pointer_is_safe(void)
{
    mock_microros_deallocate(NULL, NULL);
    TEST_PASS();
}

void test_multiple_allocations_tracking(void)
{
    void *ptr1 = mock_microros_allocate(64, NULL);
    void *ptr2 = mock_microros_allocate(128, NULL);
    void *ptr3 = mock_microros_allocate(256, NULL);
    
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NOT_NULL(ptr3);
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(3, stats->total_allocations);
    TEST_ASSERT_EQUAL_UINT(3, stats->current_allocated_blocks);
    TEST_ASSERT_EQUAL_UINT(64 + 128 + 256, stats->current_bytes_in_use);
    TEST_ASSERT_EQUAL_UINT(3, stats->peak_allocated_blocks);
    
    mock_microros_deallocate(ptr1, NULL);
    mock_microros_deallocate(ptr2, NULL);
    mock_microros_deallocate(ptr3, NULL);
    
    TEST_ASSERT_EQUAL_UINT(0, stats->current_allocated_blocks);
    TEST_ASSERT_EQUAL_UINT(0, stats->current_bytes_in_use);
}

void test_peak_stats_tracking(void)
{
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    
    void *ptr1 = mock_microros_allocate(100, NULL);
    void *ptr2 = mock_microros_allocate(200, NULL);
    
    TEST_ASSERT_EQUAL_UINT(2, stats->peak_allocated_blocks);
    TEST_ASSERT_EQUAL_UINT(300, stats->peak_bytes_in_use);
    
    mock_microros_deallocate(ptr1, NULL);
    
    TEST_ASSERT_EQUAL_UINT(2, stats->peak_allocated_blocks);
    TEST_ASSERT_EQUAL_UINT(300, stats->peak_bytes_in_use);
    
    mock_microros_deallocate(ptr2, NULL);
}

void test_allocate_failure_injection(void)
{
    mock_microros_allocator_set_fail_next(true);
    
    void *ptr = mock_microros_allocate(128, NULL);
    TEST_ASSERT_NULL(ptr);
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(1, stats->allocation_fail_count);
}

void test_allocate_failure_after_n_allocations(void)
{
    mock_microros_allocator_set_fail_n_allocations(3);
    
    void *ptr1 = mock_microros_allocate(64, NULL);
    void *ptr2 = mock_microros_allocate(64, NULL);
    void *ptr3 = mock_microros_allocate(64, NULL);
    
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_NULL(ptr3);
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(1, stats->allocation_fail_count);
    
    mock_microros_deallocate(ptr1, NULL);
    mock_microros_deallocate(ptr2, NULL);
}

void test_reallocate_null_behaves_like_allocate(void)
{
    void *ptr = mock_microros_reallocate(NULL, 128, NULL);
    TEST_ASSERT_NOT_NULL(ptr);
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(1, stats->total_allocations);
    
    mock_microros_deallocate(ptr, NULL);
}

void test_reallocate_to_zero_frees_memory(void)
{
    void *ptr = mock_microros_allocate(128, NULL);
    TEST_ASSERT_NOT_NULL(ptr);
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(1, stats->current_allocated_blocks);
    
    void *result = mock_microros_reallocate(ptr, 0, NULL);
    TEST_ASSERT_NULL(result);
    
    TEST_ASSERT_EQUAL_UINT(0, stats->current_allocated_blocks);
}

void test_reallocate_grows_memory(void)
{
    void *ptr = mock_microros_allocate(64, NULL);
    TEST_ASSERT_NOT_NULL(ptr);
    memset(ptr, 0xAB, 64);
    
    void *new_ptr = mock_microros_reallocate(ptr, 256, NULL);
    TEST_ASSERT_NOT_NULL(new_ptr);
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(1, stats->total_reallocations);
    TEST_ASSERT_EQUAL_UINT(1, stats->current_allocated_blocks);
    TEST_ASSERT_EQUAL_UINT(256, stats->current_bytes_in_use);
    
    unsigned char *bytes = (unsigned char *)new_ptr;
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_EQUAL_UINT8(0xAB, bytes[i]);
    }
    
    mock_microros_deallocate(new_ptr, NULL);
}

void test_reallocate_shrinks_memory(void)
{
    void *ptr = mock_microros_allocate(256, NULL);
    TEST_ASSERT_NOT_NULL(ptr);
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(256, stats->current_bytes_in_use);
    
    void *new_ptr = mock_microros_reallocate(ptr, 64, NULL);
    TEST_ASSERT_NOT_NULL(new_ptr);
    
    TEST_ASSERT_EQUAL_UINT(64, stats->current_bytes_in_use);
    
    mock_microros_deallocate(new_ptr, NULL);
}

void test_reallocate_failure_injection(void)
{
    void *ptr = mock_microros_allocate(64, NULL);
    TEST_ASSERT_NOT_NULL(ptr);
    
    mock_microros_allocator_set_fail_next(true);
    
    void *new_ptr = mock_microros_reallocate(ptr, 256, NULL);
    TEST_ASSERT_NULL(new_ptr);
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(1, stats->allocation_fail_count);
    
    mock_microros_deallocate(ptr, NULL);
}

void test_zero_allocate_zeros_memory(void)
{
    void *ptr = mock_microros_zero_allocate(16, 4, NULL);
    TEST_ASSERT_NOT_NULL(ptr);
    
    unsigned char *bytes = (unsigned char *)ptr;
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, bytes[i]);
    }
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(1, stats->total_zero_allocations);
    
    mock_microros_deallocate(ptr, NULL);
}

void test_zero_allocate_with_zero_elements_returns_null(void)
{
    void *ptr = mock_microros_zero_allocate(0, 4, NULL);
    TEST_ASSERT_NULL(ptr);
}

void test_zero_allocate_with_zero_size_element_returns_null(void)
{
    void *ptr = mock_microros_zero_allocate(10, 0, NULL);
    TEST_ASSERT_NULL(ptr);
}

void test_get_used_memory_functions(void)
{
    void *ptr1 = mock_microros_allocate(100, NULL);
    void *ptr2 = mock_microros_allocate(200, NULL);
    
    int used = mock_microros_get_used_memory();
    TEST_ASSERT_EQUAL_INT(300, used);
    
    int absolute = mock_microros_get_absolute_used_memory();
    TEST_ASSERT_EQUAL_INT(300, absolute);
    
    mock_microros_deallocate(ptr1, NULL);
    
    used = mock_microros_get_used_memory();
    TEST_ASSERT_EQUAL_INT(200, used);
    
    mock_microros_deallocate(ptr2, NULL);
}

void test_get_allocated_blocks(void)
{
    TEST_ASSERT_EQUAL_UINT(0, mock_microros_allocator_get_allocated_blocks());
    
    void *ptr1 = mock_microros_allocate(64, NULL);
    TEST_ASSERT_EQUAL_UINT(1, mock_microros_allocator_get_allocated_blocks());
    
    void *ptr2 = mock_microros_allocate(64, NULL);
    TEST_ASSERT_EQUAL_UINT(2, mock_microros_allocator_get_allocated_blocks());
    
    mock_microros_deallocate(ptr1, NULL);
    TEST_ASSERT_EQUAL_UINT(1, mock_microros_allocator_get_allocated_blocks());
    
    mock_microros_deallocate(ptr2, NULL);
    TEST_ASSERT_EQUAL_UINT(0, mock_microros_allocator_get_allocated_blocks());
}

void test_get_bytes_in_use(void)
{
    TEST_ASSERT_EQUAL_UINT(0, mock_microros_allocator_get_bytes_in_use());
    
    void *ptr = mock_microros_allocate(512, NULL);
    TEST_ASSERT_EQUAL_UINT(512, mock_microros_allocator_get_bytes_in_use());
    
    mock_microros_deallocate(ptr, NULL);
    TEST_ASSERT_EQUAL_UINT(0, mock_microros_allocator_get_bytes_in_use());
}

void test_deallocate_unknown_pointer_is_safe(void)
{
    void *fake_ptr = (void*)0xDEADBEEF;
    mock_microros_deallocate(fake_ptr, NULL);
    TEST_PASS();
}

void test_state_persists_across_operations(void)
{
    void *ptr1 = mock_microros_allocate(100, NULL);
    void *ptr2 = mock_microros_allocate(200, NULL);
    mock_microros_deallocate(ptr1, NULL);
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(2, stats->total_allocations);
    TEST_ASSERT_EQUAL_UINT(1, stats->total_deallocations);
    TEST_ASSERT_EQUAL_UINT(1, stats->current_allocated_blocks);
    TEST_ASSERT_EQUAL_UINT(200, stats->current_bytes_in_use);
    
    mock_microros_deallocate(ptr2, NULL);
}

void test_reset_clears_all_state(void)
{
    void *ptr1 = mock_microros_allocate(100, NULL);
    mock_microros_allocate(200, NULL);
    mock_microros_deallocate(ptr1, NULL);
    
    mock_microros_allocator_reset();
    
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    TEST_ASSERT_EQUAL_UINT(0, stats->total_allocations);
    TEST_ASSERT_EQUAL_UINT(0, stats->total_deallocations);
    TEST_ASSERT_EQUAL_UINT(0, stats->current_allocated_blocks);
    TEST_ASSERT_EQUAL_UINT(0, stats->total_bytes_allocated);
    TEST_ASSERT_EQUAL_UINT(0, stats->current_bytes_in_use);
    TEST_ASSERT_EQUAL_UINT(0, stats->allocation_fail_count);
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_allocate_returns_valid_pointer);
    RUN_TEST(test_allocate_zero_size_returns_null);
    RUN_TEST(test_allocate_updates_stats);
    RUN_TEST(test_deallocate_updates_stats);
    RUN_TEST(test_deallocate_null_pointer_is_safe);
    RUN_TEST(test_multiple_allocations_tracking);
    RUN_TEST(test_peak_stats_tracking);
    RUN_TEST(test_allocate_failure_injection);
    RUN_TEST(test_allocate_failure_after_n_allocations);
    RUN_TEST(test_reallocate_null_behaves_like_allocate);
    RUN_TEST(test_reallocate_to_zero_frees_memory);
    RUN_TEST(test_reallocate_grows_memory);
    RUN_TEST(test_reallocate_shrinks_memory);
    RUN_TEST(test_reallocate_failure_injection);
    RUN_TEST(test_zero_allocate_zeros_memory);
    RUN_TEST(test_zero_allocate_with_zero_elements_returns_null);
    RUN_TEST(test_zero_allocate_with_zero_size_element_returns_null);
    RUN_TEST(test_get_used_memory_functions);
    RUN_TEST(test_get_allocated_blocks);
    RUN_TEST(test_get_bytes_in_use);
    RUN_TEST(test_deallocate_unknown_pointer_is_safe);
    RUN_TEST(test_state_persists_across_operations);
    RUN_TEST(test_reset_clears_all_state);
    
    return UNITY_END();
}
