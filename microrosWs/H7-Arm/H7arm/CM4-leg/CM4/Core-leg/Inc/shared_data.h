#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdint.h>

#define SHARED_DATA_BASE   (0x38000000UL)
#define HSEM_ID_SENSOR     (1U)

typedef struct
{
    volatile uint32_t distance_cm;
    volatile uint32_t data_ready;
    volatile uint32_t cm4_write_seq;
    volatile uint32_t cm4_last_echo_ok;
    volatile uint32_t cm4_last_echo_ticks;
    volatile uint32_t cm4_last_wait_timeout;
    volatile uint32_t cm4_last_pulse_timeout;
    volatile uint32_t cm4_last_measurement_valid;
} shared_data_t;

#define SHARED_DATA ((shared_data_t *)SHARED_DATA_BASE)

#endif
