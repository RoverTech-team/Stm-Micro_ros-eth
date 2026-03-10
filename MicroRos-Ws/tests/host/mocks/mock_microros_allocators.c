#include "mock_microros_allocators.h"
#include <stdlib.h>
#include <string.h>

#define MAX_TRACKED_ALLOCATIONS 256

typedef struct {
    void *ptr;
    size_t size;
    bool in_use;
} allocation_entry_t;

static microros_allocator_stats_t stats = {0};
static allocation_entry_t allocation_table[MAX_TRACKED_ALLOCATIONS] = {0};
static bool fail_next_allocation = false;
static size_t fail_after_n_allocations = 0;
static size_t allocation_counter = 0;

static allocation_entry_t* find_entry(void *ptr) {
    for (size_t i = 0; i < MAX_TRACKED_ALLOCATIONS; i++) {
        if (allocation_table[i].ptr == ptr && allocation_table[i].in_use) {
            return &allocation_table[i];
        }
    }
    return NULL;
}

static allocation_entry_t* find_free_entry(void) {
    for (size_t i = 0; i < MAX_TRACKED_ALLOCATIONS; i++) {
        if (!allocation_table[i].in_use) {
            return &allocation_table[i];
        }
    }
    return NULL;
}

static void update_peak_stats(void) {
    if (stats.current_allocated_blocks > stats.peak_allocated_blocks) {
        stats.peak_allocated_blocks = stats.current_allocated_blocks;
    }
    if (stats.current_bytes_in_use > stats.peak_bytes_in_use) {
        stats.peak_bytes_in_use = stats.current_bytes_in_use;
    }
}

void mock_microros_allocator_reset(void) {
    for (size_t i = 0; i < MAX_TRACKED_ALLOCATIONS; i++) {
        if (allocation_table[i].in_use && allocation_table[i].ptr != NULL) {
            free(allocation_table[i].ptr);
        }
        allocation_table[i].ptr = NULL;
        allocation_table[i].size = 0;
        allocation_table[i].in_use = false;
    }
    memset(&stats, 0, sizeof(stats));
    fail_next_allocation = false;
    fail_after_n_allocations = 0;
    allocation_counter = 0;
}

void mock_microros_allocator_set_fail_next(bool fail) {
    fail_next_allocation = fail;
}

void mock_microros_allocator_set_fail_n_allocations(size_t n) {
    fail_after_n_allocations = n;
}

const microros_allocator_stats_t* mock_microros_allocator_get_stats(void) {
    return &stats;
}

size_t mock_microros_allocator_get_allocated_blocks(void) {
    return stats.current_allocated_blocks;
}

size_t mock_microros_allocator_get_bytes_in_use(void) {
    return stats.current_bytes_in_use;
}

void* mock_microros_allocate(size_t size, void *state) {
    (void)state;
    
    allocation_counter++;
    
    if (fail_next_allocation || 
        (fail_after_n_allocations > 0 && allocation_counter >= fail_after_n_allocations)) {
        fail_next_allocation = false;
        stats.allocation_fail_count++;
        return NULL;
    }
    
    if (size == 0) {
        return NULL;
    }
    
    void *ptr = malloc(size);
    if (ptr == NULL) {
        stats.allocation_fail_count++;
        return NULL;
    }
    
    allocation_entry_t *entry = find_free_entry();
    if (entry == NULL) {
        free(ptr);
        stats.allocation_fail_count++;
        return NULL;
    }
    
    entry->ptr = ptr;
    entry->size = size;
    entry->in_use = true;
    
    stats.total_allocations++;
    stats.current_allocated_blocks++;
    stats.total_bytes_allocated += size;
    stats.current_bytes_in_use += size;
    stats.last_allocation_size = size;
    
    update_peak_stats();
    
    return ptr;
}

void mock_microros_deallocate(void *pointer, void *state) {
    (void)state;
    
    if (pointer == NULL) {
        return;
    }
    
    allocation_entry_t *entry = find_entry(pointer);
    if (entry == NULL) {
        return;
    }
    
    stats.total_deallocations++;
    stats.current_allocated_blocks--;
    stats.total_bytes_freed += entry->size;
    stats.current_bytes_in_use -= entry->size;
    stats.last_deallocation_size = entry->size;
    
    entry->in_use = false;
    entry->ptr = NULL;
    stats.last_deallocation_size = entry->size;
    entry->size = 0;
    
    free(pointer);
}

void* mock_microros_reallocate(void *pointer, size_t size, void *state) {
    (void)state;
    
    allocation_counter++;
    
    if (size == 0) {
        if (pointer != NULL) {
            mock_microros_deallocate(pointer, state);
        }
        return NULL;
    }
    
    if (pointer == NULL) {
        return mock_microros_allocate(size, state);
    }
    
    if (fail_next_allocation) {
        fail_next_allocation = false;
        stats.allocation_fail_count++;
        return NULL;
    }
    
    allocation_entry_t *entry = find_entry(pointer);
    if (entry == NULL) {
        return NULL;
    }
    
    size_t old_size = entry->size;
    
    void *new_ptr = realloc(pointer, size);
    if (new_ptr == NULL) {
        stats.allocation_fail_count++;
        return NULL;
    }
    
    entry->ptr = new_ptr;
    entry->size = size;
    
    stats.total_reallocations++;
    if (size > old_size) {
        stats.total_bytes_allocated += (size - old_size);
        stats.current_bytes_in_use += (size - old_size);
    } else {
        stats.total_bytes_freed += (old_size - size);
        stats.current_bytes_in_use -= (old_size - size);
    }
    
    update_peak_stats();
    
    return new_ptr;
}

void* mock_microros_zero_allocate(size_t number_of_elements, size_t size_of_element, void *state) {
    size_t total_size = number_of_elements * size_of_element;
    
    void *ptr = mock_microros_allocate(total_size, state);
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
        stats.total_zero_allocations++;
    }
    
    return ptr;
}

int mock_microros_get_absolute_used_memory(void) {
    return (int)stats.total_bytes_allocated;
}

int mock_microros_get_used_memory(void) {
    return (int)stats.current_bytes_in_use;
}
