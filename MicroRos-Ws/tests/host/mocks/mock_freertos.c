#include "mock_freertos.h"
#include <string.h>

/* Internal state */
static uint32_t delay_count = 0;
static uint32_t semaphore_count = 0;

void mock_freertos_reset(void) {
    delay_count = 0;
    semaphore_count = 0;
}

uint32_t mock_freertos_get_delay_count(void) {
    return delay_count;
}

osThreadId_t osThreadNew(void (*func)(void*), void* argument, const osThreadAttr_t* attr) {
    (void)func;     /* Unused in mock */
    (void)argument; /* Unused in mock */
    (void)attr;     /* Unused in mock */
    /* Return a non-NULL fake thread handle */
    return (osThreadId_t)(uintptr_t)0xDEADBEEF;
}

osStatus_t osDelay(uint32_t millisec) {
    (void)millisec; /* Unused in mock */
    delay_count++;
    return osOK;
}

osSemaphoreId_t osSemaphoreNew(uint32_t max_count, uint32_t initial_count, void* attr) {
    (void)max_count;     /* Unused in mock */
    (void)initial_count; /* Unused in mock */
    (void)attr;          /* Unused in mock */
    semaphore_count++;
    /* Return a fake semaphore handle */
    return (osSemaphoreId_t)(uintptr_t)(0x1000 + semaphore_count);
}

osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore_id, uint32_t timeout) {
    (void)semaphore_id; /* Unused in mock */
    (void)timeout;      /* Unused in mock */
    /* Always succeed for testing */
    return osOK;
}

osStatus_t osSemaphoreRelease(osSemaphoreId_t semaphore_id) {
    (void)semaphore_id; /* Unused in mock */
    /* Always succeed for testing */
    return osOK;
}
