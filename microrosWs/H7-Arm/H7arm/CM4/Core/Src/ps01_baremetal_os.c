/* ================================================================ */
/*  ps01_baremetal_os.c  -  Bare-metal OS glue for powerSTEP01     */
/*                                                                  */
/*  Implements the PS01_OS_t interface for the STM32H755 CM4 core  */
/*  running bare-metal (no RTOS).                                  */
/* ================================================================ */

#include "drivers/powerSTEP01/ps01.h"
#include "stm32h7xx_hal.h"

const PS01_OS_t *ps01_baremetal_get_os(void);

/* ---- Atomic spinlock (does NOT disable interrupts) ---------------
 *  LDREXB/STREXB exclusive-access instructions provide mutual
 *  exclusion on single-core CM4 without killing SysTick.  __WFE()
 *  pauses the core during contention; __SEV() from the release path
 *  wakes it immediately.                                       */

static volatile uint8_t bm_spinlock = 0;

static void bm_mutex_acquire(void *arg)
{
    (void)arg;
    for (;;) {
        if (__LDREXB(&bm_spinlock) == 0) {
            if (__STREXB(1, &bm_spinlock) == 0) {
                __DMB();
                return;
            }
        }
        __WFE();
    }
}

static void bm_mutex_release(void *arg)
{
    (void)arg;
    __DMB();
    bm_spinlock = 0;
    __SEV();
}

static void bm_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ---- The OS interface instance ---- */
static const PS01_OS_t ps01_os = {
    .mutex             = NULL,
    .mutex_acquire     = bm_mutex_acquire,
    .mutex_release     = bm_mutex_release,
    .delay_ms          = bm_delay_ms,
};



/* ---- Public init function ---- */

const PS01_OS_t *ps01_baremetal_get_os(void)
{
    return &ps01_os;
}
