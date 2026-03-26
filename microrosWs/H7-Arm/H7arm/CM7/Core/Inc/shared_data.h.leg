#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdint.h>

#define SHARED_DATA_BASE   (0x38000000UL)
#define HSEM_ID_SENSOR     (0U)

typedef struct
{
    volatile uint32_t distance_cm;
    volatile uint32_t data_ready;
} shared_data_t;

#define SHARED_DATA ((shared_data_t *)SHARED_DATA_BASE)

#endif
