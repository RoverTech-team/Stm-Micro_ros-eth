#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H

#include <stdint.h>
#include <stdbool.h>

/* Mock CMSIS-RTOS types */
typedef void* osThreadId_t;
typedef void* osSemaphoreId_t;

typedef struct {
    const char *name;
    uint32_t stack_size;
    int priority;
} osThreadAttr_t;

typedef enum {
    osOK = 0,
    osErrorTimeout = -1,
    osErrorParameter = -2,
    osErrorResource = -3
} osStatus_t;

typedef enum {
    osPriorityNormal = 24,
    osPriorityBelowNormal = 16,
    osPriorityRealtime = 48
} osPriority_t;

/* Mock FreeRTOS functions */
osThreadId_t osThreadNew(void (*func)(void*), void* argument, const osThreadAttr_t* attr);
osStatus_t osDelay(uint32_t millisec);
osSemaphoreId_t osSemaphoreNew(uint32_t max_count, uint32_t initial_count, void* attr);
osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore_id, uint32_t timeout);
osStatus_t osSemaphoreRelease(osSemaphoreId_t semaphore_id);

/* Mock control functions */
void mock_freertos_reset(void);
uint32_t mock_freertos_get_delay_count(void);

#endif /* MOCK_FREERTOS_H */
