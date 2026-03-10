#include "mock_memory.h"
#include <stdlib.h>
#include <string.h>

/* Internal state tracking */
static size_t allocation_count = 0;
static size_t total_allocated_bytes = 0;

void mock_memory_reset(void) {
    allocation_count = 0;
    total_allocated_bytes = 0;
}

size_t mock_memory_get_allocation_count(void) {
    return allocation_count;
}

size_t mock_memory_get_allocated_bytes(void) {
    return total_allocated_bytes;
}

void *mock_memory_allocate(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    void *ptr = malloc(size);
    if (ptr != NULL) {
        allocation_count++;
        total_allocated_bytes += size;
    }
    return ptr;
}

void mock_memory_deallocate(void *ptr) {
    if (ptr != NULL) {
        free(ptr);
        allocation_count--;
    }
}

void *mock_memory_reallocate(void *ptr, size_t new_size) {
    if (new_size == 0) {
        /* realloc with size 0 is equivalent to free */
        if (ptr != NULL) {
            free(ptr);
            allocation_count--;
        }
        return NULL;
    }
    
    if (ptr == NULL) {
        /* realloc with NULL ptr is equivalent to malloc */
        return mock_memory_allocate(new_size);
    }
    
    void *new_ptr = realloc(ptr, new_size);
    if (new_ptr != NULL) {
        /* Don't increment allocation_count since it's a reallocation */
        total_allocated_bytes += new_size; /* Simplified tracking */
    }
    return new_ptr;
}

void *mock_memory_zero_allocate(size_t size) {
    void *ptr = mock_memory_allocate(size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}
