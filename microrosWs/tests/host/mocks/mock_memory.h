#ifndef MOCK_MEMORY_H
#define MOCK_MEMORY_H

#include <stddef.h>

/* Mock memory allocation functions for testing micro-ROS allocators */
void *mock_memory_allocate(size_t size);
void mock_memory_deallocate(void *ptr);
void *mock_memory_reallocate(void *ptr, size_t new_size);
void *mock_memory_zero_allocate(size_t size);

/* Mock control functions */
void mock_memory_reset(void);
size_t mock_memory_get_allocation_count(void);
size_t mock_memory_get_allocated_bytes(void);

#endif /* MOCK_MEMORY_H */
