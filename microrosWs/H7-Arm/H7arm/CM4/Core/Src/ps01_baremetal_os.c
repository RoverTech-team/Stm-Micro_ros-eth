#include "drivers/powerSTEP01/ps01.h"
#include "stm32h7xx_hal.h"

const PS01_OS_t *ps01_baremetal_get_os(void);

static void bm_semaphore_acquire(void *arg) { (void)arg; }
static void bm_delay_ms(uint32_t ms)        { HAL_Delay(ms); }

static const PS01_OS_t ps01_os = {
    .semaphore         = NULL,
    .semaphore_acquire = bm_semaphore_acquire,
    .delay_ms          = bm_delay_ms,
};

const PS01_OS_t *ps01_baremetal_get_os(void)
{
    return &ps01_os;
}
