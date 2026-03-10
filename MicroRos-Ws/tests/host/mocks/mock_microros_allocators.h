#ifndef MOCK_MICROROS_ALLOCATORS_H
#define MOCK_MICROROS_ALLOCATORS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    size_t total_allocations;
    size_t total_deallocations;
    size_t total_reallocations;
    size_t total_zero_allocations;
    size_t current_allocated_blocks;
    size_t peak_allocated_blocks;
    size_t total_bytes_allocated;
    size_t total_bytes_freed;
    size_t current_bytes_in_use;
    size_t peak_bytes_in_use;
    size_t allocation_fail_count;
    size_t last_allocation_size;
    size_t last_deallocation_size;
} microros_allocator_stats_t;

void mock_microros_allocator_reset(void);

void mock_microros_allocator_set_fail_next(bool fail);
void mock_microros_allocator_set_fail_n_allocations(size_t n);

const microros_allocator_stats_t* mock_microros_allocator_get_stats(void);
size_t mock_microros_allocator_get_allocated_blocks(void);
size_t mock_microros_allocator_get_bytes_in_use(void);

void* mock_microros_allocate(size_t size, void *state);
void mock_microros_deallocate(void *pointer, void *state);
void* mock_microros_reallocate(void *pointer, size_t size, void *state);
void* mock_microros_zero_allocate(size_t number_of_elements, size_t size_of_element, void *state);

int mock_microros_get_absolute_used_memory(void);
int mock_microros_get_used_memory(void);

#endif
