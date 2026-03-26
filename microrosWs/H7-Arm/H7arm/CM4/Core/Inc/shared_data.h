#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdint.h>

/**
 * Struttura dati condivisa tra CM4 e CM7 nella SRAM4 (D3 domain).
 * Indirizzo: 0x38000000, dimensione disponibile: 64 KB.
 *
 * Il CM4 scrive distance_cm e alza data_ready.
 * Il CM7 legge distance_cm e abbassa data_ready.
 * I campi sono volatile perché modificati da un core diverso.
 */
typedef struct
{
  volatile uint32_t distance_cm;   /* distanza misurata dal sensore [cm] */
  volatile uint32_t data_ready;    /* 1 = dato nuovo disponibile         */
} SharedData_t;

/* Puntatore alla struttura in SRAM4 */
#define SHARED_DATA  ((SharedData_t *)0x38000000U)

/* ID del semaforo hardware usato per segnalare CM7 */
#define HSEM_ID_SENSOR  1U

#endif /* SHARED_DATA_H */
