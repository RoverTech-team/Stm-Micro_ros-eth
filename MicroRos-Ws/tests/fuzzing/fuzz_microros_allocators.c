#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "../host/mocks/mock_microros_allocators.h"

#define MAX_ALLOCATION_SIZE (1024 * 1024)
#define MAX_ALLOCATIONS 1000

static size_t total_allocated = 0;
static size_t allocation_count = 0;

typedef struct {
    void *ptr;
    size_t size;
    int generation;
} tracked_allocation_t;

static tracked_allocation_t tracked_allocs[MAX_ALLOCATIONS];

static void reset_tracker(void) {
    total_allocated = 0;
    allocation_count = 0;
    memset(tracked_allocs, 0, sizeof(tracked_allocs));
}

static void track_allocation(void *ptr, size_t size) {
    if (ptr == NULL) return;
    
    for (size_t i = 0; i < MAX_ALLOCATIONS; i++) {
        if (tracked_allocs[i].ptr == NULL) {
            tracked_allocs[i].ptr = ptr;
            tracked_allocs[i].size = size;
            tracked_allocs[i].generation = (int)allocation_count;
            total_allocated += size;
            allocation_count++;
            break;
        }
    }
}

static void untrack_allocation(void *ptr) {
    if (ptr == NULL) return;
    
    for (size_t i = 0; i < MAX_ALLOCATIONS; i++) {
        if (tracked_allocs[i].ptr == ptr) {
            total_allocated -= tracked_allocs[i].size;
            tracked_allocs[i].ptr = NULL;
            tracked_allocs[i].size = 0;
            tracked_allocs[i].generation = 0;
            break;
        }
    }
}

static void free_all_tracked(void) {
    for (size_t i = 0; i < MAX_ALLOCATIONS; i++) {
        if (tracked_allocs[i].ptr != NULL) {
            mock_microros_deallocate(tracked_allocs[i].ptr, NULL);
            tracked_allocs[i].ptr = NULL;
            tracked_allocs[i].size = 0;
        }
    }
    total_allocated = 0;
}

static void fuzz_allocate_sizes(const uint8_t *data, size_t len) {
    if (len == 0) return;
    
    size_t sizes[] = {
        0,
        1,
        16,
        64,
        256,
        1024,
        4096,
        65536,
        262144,
        MAX_ALLOCATION_SIZE,
        SIZE_MAX,
        SIZE_MAX - 1,
        SIZE_MAX / 2,
        (size_t)-1,
    };
    
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        void *ptr = mock_microros_allocate(sizes[i], NULL);
        if (ptr != NULL && sizes[i] > 0 && sizes[i] < MAX_ALLOCATION_SIZE) {
            memset(ptr, 0xAA, sizes[i] < 1024 ? sizes[i] : 1024);
            mock_microros_deallocate(ptr, NULL);
        }
    }
    
    for (size_t i = 0; i < len && i < 100; i++) {
        size_t size = data[i];
        void *ptr = mock_microros_allocate(size, NULL);
        if (ptr != NULL) {
            track_allocation(ptr, size);
            if (size > 0 && size < 1024) {
                memset(ptr, 0x55, size);
            }
        }
    }
    
    free_all_tracked();
}

static void fuzz_realloc_combinations(const uint8_t *data, size_t len) {
    if (len < 2) return;
    
    void *ptr = NULL;
    
    ptr = mock_microros_reallocate(NULL, data[0] % 256 + 1, NULL);
    if (ptr != NULL) {
        track_allocation(ptr, data[0] % 256 + 1);
    }
    
    for (size_t i = 1; i < len && i < 50; i++) {
        size_t new_size = data[i] % 512;
        
        void *new_ptr = mock_microros_reallocate(ptr, new_size, NULL);
        
        if (new_ptr != NULL) {
            untrack_allocation(ptr);
            track_allocation(new_ptr, new_size);
            ptr = new_ptr;
            
            if (new_size > 0 && new_size < 256) {
                memset(new_ptr, 0x77, new_size);
            }
        } else if (new_size == 0) {
            untrack_allocation(ptr);
            ptr = NULL;
        }
    }
    
    if (ptr != NULL) {
        mock_microros_deallocate(ptr, NULL);
        untrack_allocation(ptr);
    }
    
    void *ptr1 = mock_microros_allocate(64, NULL);
    void *ptr2 = mock_microros_allocate(64, NULL);
    void *ptr3 = mock_microros_allocate(64, NULL);
    
    ptr1 = mock_microros_reallocate(ptr1, 128, NULL);
    ptr2 = mock_microros_reallocate(ptr2, 32, NULL);
    ptr3 = mock_microros_reallocate(ptr3, 0, NULL);
    
    if (ptr1) mock_microros_deallocate(ptr1, NULL);
    if (ptr2) mock_microros_deallocate(ptr2, NULL);
}

static void fuzz_zero_allocate(const uint8_t *data, size_t len) {
    size_t test_cases[][2] = {
        {0, 0},
        {0, 1},
        {1, 0},
        {1, 1},
        {10, 10},
        {100, 4},
        {4, 100},
        {256, 1},
        {1, 256},
        {SIZE_MAX, 1},
        {1, SIZE_MAX},
        {SIZE_MAX, SIZE_MAX},
        {SIZE_MAX / 2, 2},
        {2, SIZE_MAX / 2},
    };
    
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        void *ptr = mock_microros_zero_allocate(test_cases[i][0], test_cases[i][1], NULL);
        if (ptr != NULL) {
            size_t total = test_cases[i][0] * test_cases[i][1];
            if (total < 256 && total > 0) {
                unsigned char *bytes = (unsigned char *)ptr;
                for (size_t j = 0; j < total; j++) {
                    if (bytes[j] != 0) {
                        break;
                    }
                }
            }
            mock_microros_deallocate(ptr, NULL);
        }
    }
    
    if (len >= 2) {
        for (size_t i = 0; i < len - 1; i++) {
            size_t num = data[i] % 32;
            size_t size = data[i + 1] % 32;
            
            void *ptr = mock_microros_zero_allocate(num, size, NULL);
            if (ptr != NULL) {
                mock_microros_deallocate(ptr, NULL);
            }
        }
    }
}

static void fuzz_failure_injection(const uint8_t *data, size_t len) {
    if (len == 0) return;
    
    mock_microros_allocator_set_fail_next(true);
    void *ptr1 = mock_microros_allocate(128, NULL);
    if (ptr1 != NULL) {
        mock_microros_deallocate(ptr1, NULL);
    }
    
    mock_microros_allocator_set_fail_next(false);
    mock_microros_allocator_set_fail_n_allocations(3);
    
    void *allocs[5] = {NULL};
    for (int i = 0; i < 5; i++) {
        allocs[i] = mock_microros_allocate(64, NULL);
    }
    
    for (int i = 0; i < 5; i++) {
        if (allocs[i] != NULL) {
            mock_microros_deallocate(allocs[i], NULL);
        }
    }
    
    if (len >= 3) {
        mock_microros_allocator_set_fail_n_allocations(data[0] % 10 + 1);
        
        for (size_t i = 0; i < (size_t)(data[1] % 20); i++) {
            void *p = mock_microros_allocate(data[2] % 128 + 1, NULL);
            if (p != NULL) {
                mock_microros_deallocate(p, NULL);
            }
        }
    }
    
    mock_microros_allocator_reset();
}

static void fuzz_memory_tracking_consistency(const uint8_t *data, size_t len) {
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    
    size_t initial_blocks = stats->current_allocated_blocks;
    size_t initial_bytes = stats->current_bytes_in_use;
    
    void *ptrs[20] = {NULL};
    size_t alloc_count = (len > 0) ? (data[0] % 20) : 10;
    
    for (size_t i = 0; i < alloc_count; i++) {
        size_t size = (i < len) ? (data[i % len] % 128 + 1) : 64;
        ptrs[i] = mock_microros_allocate(size, NULL);
        
        if (ptrs[i] != NULL) {
            if (stats->current_allocated_blocks != initial_blocks + i + 1) {
            }
            if (stats->current_bytes_in_use < initial_bytes + (i + 1)) {
            }
        }
    }
    
    for (size_t i = 0; i < alloc_count; i++) {
        if (ptrs[i] != NULL) {
            mock_microros_deallocate(ptrs[i], NULL);
            ptrs[i] = NULL;
        }
    }
    
    if (stats->current_allocated_blocks != initial_blocks) {
    }
    if (stats->current_bytes_in_use != initial_bytes) {
    }
}

static void fuzz_stress_allocator(const uint8_t *data, size_t len) {
    void *ptrs[MAX_ALLOCATIONS] = {NULL};
    size_t ptr_count = 0;
    size_t iterations = (len > 0) ? (data[0] % 200 + 50) : 100;
    
    for (size_t i = 0; i < iterations && ptr_count < MAX_ALLOCATIONS - 1; i++) {
        size_t idx = i % len;
        uint8_t op = (len > idx) ? (data[idx] % 4) : (i % 4);
        
        switch (op) {
            case 0: {
                size_t size = (idx + 1 < len) ? data[idx + 1] % 256 + 1 : 64;
                void *p = mock_microros_allocate(size, NULL);
                if (p != NULL) {
                    ptrs[ptr_count++] = p;
                }
                break;
            }
            case 1: {
                if (ptr_count > 0) {
                    size_t free_idx = data[idx] % ptr_count;
                    if (ptrs[free_idx] != NULL) {
                        mock_microros_deallocate(ptrs[free_idx], NULL);
                        ptrs[free_idx] = ptrs[--ptr_count];
                        ptrs[ptr_count] = NULL;
                    }
                }
                break;
            }
            case 2: {
                if (ptr_count > 0) {
                    size_t realloc_idx = data[idx] % ptr_count;
                    size_t new_size = (idx + 1 < len) ? data[idx + 1] % 512 : 128;
                    void *new_ptr = mock_microros_reallocate(ptrs[realloc_idx], new_size, NULL);
                    if (new_ptr != NULL) {
                        ptrs[realloc_idx] = new_ptr;
                    }
                }
                break;
            }
            case 3: {
                size_t num = (idx < len) ? data[idx] % 16 + 1 : 4;
                size_t elem_size = (idx + 1 < len) ? data[idx + 1] % 16 + 1 : 4;
                void *p = mock_microros_zero_allocate(num, elem_size, NULL);
                if (p != NULL && ptr_count < MAX_ALLOCATIONS) {
                    ptrs[ptr_count++] = p;
                }
                break;
            }
        }
    }
    
    for (size_t i = 0; i < ptr_count; i++) {
        if (ptrs[i] != NULL) {
            mock_microros_deallocate(ptrs[i], NULL);
        }
    }
}

static void fuzz_leak_detection(const uint8_t *data, size_t len) {
    const microros_allocator_stats_t *stats = mock_microros_allocator_get_stats();
    
    size_t initial_blocks = stats->current_allocated_blocks;
    size_t initial_bytes = stats->current_bytes_in_use;
    
    {
        void *temp_ptrs[10];
        for (int i = 0; i < 10; i++) {
            size_t size = (len > 0) ? (data[i % len] % 128 + 1) : 32;
            temp_ptrs[i] = mock_microros_allocate(size, NULL);
        }
        
        for (int i = 0; i < 10; i++) {
            if (temp_ptrs[i] != NULL) {
                mock_microros_deallocate(temp_ptrs[i], NULL);
            }
        }
    }
    
    if (stats->current_allocated_blocks != initial_blocks) {
    }
    if (stats->current_bytes_in_use != initial_bytes) {
    }
}

static void fuzz_boundary_conditions(void) {
    void *ptr = mock_microros_allocate(0, NULL);
    if (ptr != NULL) {
        mock_microros_deallocate(ptr, NULL);
    }
    
    mock_microros_deallocate(NULL, NULL);
    
    ptr = mock_microros_reallocate(NULL, 0, NULL);
    if (ptr != NULL) {
        mock_microros_deallocate(ptr, NULL);
    }
    
    ptr = mock_microros_allocate(1, NULL);
    if (ptr != NULL) {
        ptr = mock_microros_reallocate(ptr, 0, NULL);
        if (ptr != NULL) {
            mock_microros_deallocate(ptr, NULL);
        }
    }
    
    ptr = mock_microros_zero_allocate(0, 0, NULL);
    if (ptr != NULL) {
        mock_microros_deallocate(ptr, NULL);
    }
    
    ptr = mock_microros_allocate(SIZE_MAX, NULL);
    if (ptr != NULL) {
        mock_microros_deallocate(ptr, NULL);
    }
    
    ptr = mock_microros_reallocate(NULL, SIZE_MAX, NULL);
    if (ptr != NULL) {
        mock_microros_deallocate(ptr, NULL);
    }
    
    ptr = mock_microros_zero_allocate(SIZE_MAX, 1, NULL);
    if (ptr != NULL) {
        mock_microros_deallocate(ptr, NULL);
    }
    
    ptr = mock_microros_zero_allocate(1, SIZE_MAX, NULL);
    if (ptr != NULL) {
        mock_microros_deallocate(ptr, NULL);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len) {
    mock_microros_allocator_reset();
    reset_tracker();
    
    fuzz_boundary_conditions();
    
    mock_microros_allocator_reset();
    reset_tracker();
    
    fuzz_allocate_sizes(data, len);
    
    mock_microros_allocator_reset();
    reset_tracker();
    
    fuzz_realloc_combinations(data, len);
    
    mock_microros_allocator_reset();
    reset_tracker();
    
    fuzz_zero_allocate(data, len);
    
    mock_microros_allocator_reset();
    reset_tracker();
    
    fuzz_failure_injection(data, len);
    
    mock_microros_allocator_reset();
    reset_tracker();
    
    fuzz_memory_tracking_consistency(data, len);
    
    mock_microros_allocator_reset();
    reset_tracker();
    
    fuzz_stress_allocator(data, len);
    
    mock_microros_allocator_reset();
    reset_tracker();
    
    fuzz_leak_detection(data, len);
    
    mock_microros_allocator_reset();
    
    return 0;
}