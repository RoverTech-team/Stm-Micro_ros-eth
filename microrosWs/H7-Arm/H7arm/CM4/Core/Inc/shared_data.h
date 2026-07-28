#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <stdint.h>

#define SHARED_MAGIC        0x53485244U  /* "SHRD" in ASCII */
#define SHARED_VERSION      2U

#define SHARED_JOINT_COUNT  6U

_Static_assert(SHARED_JOINT_COUNT == 6, "SHARED_JOINT_COUNT must match motor driver N_JOINTS");

typedef struct
{
  uint32_t magic;
  uint32_t version;

  /* --- Ultrasonic sensor fields (legacy) --- */
  volatile uint32_t distance_cm;
  volatile uint32_t data_ready;
  volatile uint32_t cm4_write_seq;
  volatile uint32_t cm4_last_echo_ok;
  volatile uint32_t cm4_last_echo_ticks;
  volatile uint32_t cm4_last_wait_timeout;
  volatile uint32_t cm4_last_pulse_timeout;
  volatile uint32_t cm4_last_measurement_valid;

  /* --- Motor command/state (protocol v2: int32_t milli-degrees) --- */
  volatile int32_t  joint_cmd_positions[SHARED_JOINT_COUNT];
  volatile int32_t  joint_act_positions[SHARED_JOINT_COUNT];

  volatile uint32_t joint_cmd_seq;
  volatile uint32_t joint_cmd_ack;

  volatile uint32_t motor_ready;
  volatile uint32_t motor_ready_seq;
  volatile uint32_t motion_done_seq;

  /* --- Fault telemetry (CM4 writes on exception) --- */
  volatile uint32_t last_fault_cfsr;
  volatile uint32_t last_fault_hfsr;
  volatile uint32_t last_fault_mmar;
  volatile uint32_t last_fault_bfar;
  volatile uint32_t last_fault_lr;
  volatile uint32_t last_fault_pc;
  volatile uint32_t last_fault_ipsr;
  volatile uint32_t last_fault_cfb;

  /* --- Driver fault alarms (byte per joint, OR of ALARM_* flags) --- */
  volatile uint32_t fault_alarm;

  /* --- Raw status register values from last PollFaults cycle --- */
  volatile uint16_t dbg_joint_status[SHARED_JOINT_COUNT];

  /* --- Last fault latch (written once, survives until reset) --- */
  volatile uint32_t last_fault_code;
  volatile uint32_t last_fault_tick;

  /* --- IMU sensor fields (LSM6DSV16X) --- */
  volatile int32_t imu_accel_x_mg;
  volatile int32_t imu_accel_y_mg;
  volatile int32_t imu_accel_z_mg;
  volatile int32_t imu_gyro_x_mdps;
  volatile int32_t imu_gyro_y_mdps;
  volatile int32_t imu_gyro_z_mdps;
  volatile int32_t imu_temp_mdegc;
  volatile uint32_t imu_data_ready;
  volatile uint32_t imu_seq;

} shared_data_t;

__attribute__((section(".shared"))) extern shared_data_t shared_data_inst;

#define SHARED_DATA  (&shared_data_inst)

#define HSEM_ID_SENSOR  1U

#endif
