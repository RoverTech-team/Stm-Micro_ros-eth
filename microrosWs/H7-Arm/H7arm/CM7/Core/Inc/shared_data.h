#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdint.h>

/**
 * Number of joints on the robotic arm.
 * Must match N_JOINTS in the motor driver (rarm.h).
 */
#define SHARED_JOINT_COUNT  6U

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
  /* --- Ultrasonic sensor fields (existing) --- */
  volatile uint32_t distance_cm;   /* distanza misurata dal sensore [cm] */
  volatile uint32_t data_ready;    /* 1 = dato nuovo disponibile         */
  volatile uint32_t cm4_write_seq;
  volatile uint32_t cm4_last_echo_ok;
  volatile uint32_t cm4_last_echo_ticks;
  volatile uint32_t cm4_last_wait_timeout;
  volatile uint32_t cm4_last_pulse_timeout;
  volatile uint32_t cm4_last_measurement_valid;

  /* --- Motor command/state fields (NEW) --- */

  /** Joint positions commanded by CM7 (degrees, written by CM7) */
  volatile float    joint_cmd_positions[SHARED_JOINT_COUNT];

  /** Actual joint positions read from motors (degrees, written by CM4) */
  volatile float    joint_act_positions[SHARED_JOINT_COUNT];

  /** Command sequence number — CM7 increments on each new command */
  volatile uint32_t joint_cmd_seq;

  /** CM4 echoes joint_cmd_seq after processing the command */
  volatile uint32_t joint_cmd_ack;

  /** CM4 sets to 1 once the motor driver is initialized and ready */
  volatile uint32_t motor_ready;

} shared_data_t;

/* Puntatore alla struttura in SRAM4 */
#define SHARED_DATA  ((shared_data_t *)0x38000000U)

/* ID del semaforo hardware usato per segnalare CM7 */
#define HSEM_ID_SENSOR  1U

#endif /* SHARED_DATA_H */
