/* ================================================================ */
/*  ps01_baremetal_os.c  -  Bare-metal OS glue for powerSTEP01     */
/*                                                                  */
/*  Implements the PS01_OS_t interface for the STM32H755 CM4 core  */
/*  running bare-metal (no RTOS).                                  */
/* ================================================================ */

#include "drivers/powerSTEP01/ps01.h"
#include "stm32h7xx_hal.h"

/* ---- OS function implementations ---- */

static void bm_mutex_acquire(void *arg)
{
    (void)arg;
    __disable_irq();
}

static void bm_mutex_release(void *arg)
{
    (void)arg;
    __enable_irq();
}

static void bm_semaphore_acquire(void *arg)
{
    (void)arg;
    /* Not used with SPI polling */
}

static void bm_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ---- The OS interface instance ---- */
static const PS01_OS_t ps01_os = {
    .mutex             = NULL,
    .semaphore         = NULL,
    .mutex_acquire     = bm_mutex_acquire,
    .mutex_release     = bm_mutex_release,
    .semaphore_acquire = bm_semaphore_acquire,
    .delay_ms          = bm_delay_ms,
};



/* ---- Public init function ---- */

const PS01_OS_t *ps01_baremetal_get_os(void)
{
    return &ps01_os;
}
