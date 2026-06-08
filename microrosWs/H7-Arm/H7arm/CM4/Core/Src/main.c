/**
 * CM4 bare-metal firmware — powerSTEP01 stepper driver.
 *
 * Reads joint commands from shared SRAM4 (written by CM7) and drives the
 * powerSTEP01 daisy-chain via SPI1. Actual joint positions are written
 * back to shared SRAM4 for CM7 to publish as /joint_states.
 *
 * Cross-core protocol: see shared_data.h. Magic/version gate is written
 * here after init. CM7 will refuse to publish commands on mismatch.
 */

#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "tim.h"
#include "drivers/RArm/rarm.h"
#include "drivers/powerSTEP01/ps01.h"
#include "shared_data.h"
#include <stdbool.h>
#include <string.h>

#ifndef WIRE_TEST
#define WIRE_TEST 1
#endif

#define MOTOR_LOOP_PERIOD_MS 10U
#define BRAKE_SETTLE_MS      100U

__attribute__((section(".shared"))) shared_data_t shared_data_inst;

extern const PS01_OS_t *ps01_baremetal_get_os(void);

static Stepper_t     motors[N_JOINTS];
static StepperBank_t bank = { .motors = motors, .active = 0 };

#if !WIRE_TEST
static uint32_t last_processed_cmd_seq = 0U;
static bool     motion_pending[N_JOINTS] = { false };
static uint32_t motion_pending_start_ms[N_JOINTS] = { 0U };

static void Motor_ProcessCommands(void);
#endif
static void Motor_UpdatePositions(void);

int main(void)
{
  extern uint32_t g_pfnVectors;
  SCB->VTOR = (uint32_t)&g_pfnVectors;

  HAL_Init();

  memset((void *)SHARED_DATA, 0, sizeof(*SHARED_DATA));
  SHARED_DATA->magic          = SHARED_MAGIC;
  SHARED_DATA->version        = SHARED_VERSION;
  SHARED_DATA->cm4_write_seq  = 1U;
  __DSB();

  MX_GPIO_Init();
  MX_TIM2_Init();

  for (int j = 0; j < 2; j++) {
    LED_GREEN_ON();
    LED_RED_ON();
    DelayMs(20);
    LED_GREEN_OFF();
    LED_RED_OFF();
    DelayMs(20);
  }

  MX_SPI1_Init();

  /* 24 V rail rise time per powerSTEP01 datasheet (rail must be stable before
   * the drivers come out of reset, otherwise the charge pump misbehaves). */
  HAL_GPIO_WritePin(DRV_RESET_GPIO_Port, DRV_RESET_Pin, GPIO_PIN_RESET);
  DelayMs(2300);
  HAL_GPIO_WritePin(DRV_RESET_GPIO_Port, DRV_RESET_Pin, GPIO_PIN_SET);
  DelayMs(100);
  DelayMs(1000);

  RARM_SetBank(&bank);
  ps01Init(ps01_baremetal_get_os());

  for (uint8_t i = 0; i < N_JOINTS; i++) {
    mot_bank->active = i;
    ps01GetStatus_chain();
  }
  mot_bank->active = J1_INDEX;

  RARM_SimpleConfig_t joint1_config = {.OVERCURRENT_SD = OC_NOSHUTDOWN,
                                       .VSCOMP = VSCOMP_DISABLE,
                                       .STEP_MODE = SM_128_MICROSTEP,
                                       .steps_rev = 200,
                                       .reduction_ratio = 3,
                                       .supply_voltage = 24.0f,
                                       .run_acc_dec_voltage = 3.5f,
                                       .hold_voltage = 2.5f,
                                       .max_speed = 100,
                                       .min_speed = 0,
                                       .acceleration = 300,
                                       .deceleration = 300,
                                       .fullstep_speed = 100,
                                       .oc_threshold = 20.0f,
                                       .stall_threshold = 20.0f,
                                       .min_degs = -180,
                                       .max_degs = 180,
                                       .st_slp = 0x21,
                                       .fn_slp_acc = 0x89,
                                       .fn_slp_dec = 0x89};
  RARM_SetConfig(J1_INDEX, &joint1_config);

  RARM_SimpleConfig_t joint4_config = {.OVERCURRENT_SD = OC_NOSHUTDOWN,
                                       .VSCOMP = VSCOMP_DISABLE,
                                       .STEP_MODE = SM_128_MICROSTEP,
                                       .steps_rev = 200,
                                       .reduction_ratio = 5,
                                       .supply_voltage = 24.0f,
                                       .run_acc_dec_voltage = 2.34f,
                                       .hold_voltage = 1.5f,
                                       .max_speed = 400,
                                       .min_speed = 0,
                                       .acceleration = 400,
                                       .deceleration = 400,
                                       .fullstep_speed = 400,
                                       .oc_threshold = 20.0f,
                                       .stall_threshold = 20.0f,
                                       .min_degs = -180,
                                       .max_degs = 180,
                                       .st_slp = 0x59,
                                       .fn_slp_acc = 0x29,
                                       .fn_slp_dec = 0x29};
  RARM_SetConfig(J4_INDEX, &joint4_config);

  RARM_SimpleConfig_t joint6_config = {.OVERCURRENT_SD = OC_NOSHUTDOWN,
                                       .VSCOMP = VSCOMP_DISABLE,
                                       .STEP_MODE = SM_128_MICROSTEP,
                                       .steps_rev = 200,
                                       .reduction_ratio = 50,
                                       .supply_voltage = 24.0f,
                                       .run_acc_dec_voltage = 2.34f,
                                       .hold_voltage = 1.5f,
                                       .max_speed = 1000,
                                       .min_speed = 0,
                                       .acceleration = 4000,
                                       .deceleration = 4000,
                                       .fullstep_speed = 1000,
                                       .oc_threshold = 20.0f,
                                       .stall_threshold = 20.0f,
                                       .min_degs = -180,
                                       .max_degs = 180,
                                       .st_slp = 0x21,
                                       .fn_slp_acc = 0x89,
                                       .fn_slp_dec = 0x89};
  RARM_SetConfig(J6_INDEX, &joint6_config);

  RARM_SimpleConfig_t joint5_config = {.OVERCURRENT_SD = OC_NOSHUTDOWN,
                                       .VSCOMP = VSCOMP_DISABLE,
                                       .STEP_MODE = SM_128_MICROSTEP,
                                       .steps_rev = 200,
                                       .reduction_ratio = 5,
                                       .supply_voltage = 24.0f,
                                       .run_acc_dec_voltage = 2.34f,
                                       .hold_voltage = 1.5f,
                                       .max_speed = 400,
                                       .min_speed = 0,
                                       .acceleration = 400,
                                       .deceleration = 400,
                                       .fullstep_speed = 400,
                                       .oc_threshold = 20.0f,
                                       .stall_threshold = 20.0f,
                                       .min_degs = -180,
                                       .max_degs = 180,
                                       .st_slp = 0x59,
                                       .fn_slp_acc = 0x29,
                                       .fn_slp_dec = 0x29};
  RARM_SetConfig(J5_INDEX, &joint5_config);

  RARM_SimpleConfig_t joint3_config = {.OVERCURRENT_SD = OC_NOSHUTDOWN,
                                       .VSCOMP = VSCOMP_DISABLE,
                                       .STEP_MODE = SM_128_MICROSTEP,
                                       .steps_rev = 200,
                                       .reduction_ratio = 50,
                                       .supply_voltage = 24.0f,
                                       .run_acc_dec_voltage = 3.5f,
                                       .hold_voltage = 3.5f,
                                       .max_speed = 1000,
                                       .min_speed = 0,
                                       .acceleration = 1000,
                                       .deceleration = 1000,
                                       .fullstep_speed = 1000,
                                       .oc_threshold = 20.0f,
                                       .stall_threshold = 20.0f,
                                       .min_degs = -180,
                                       .max_degs = 180,
                                       .st_slp = 0x89,
                                       .fn_slp_acc = 0x79,
                                       .fn_slp_dec = 0x79};
  RARM_SetConfig(J3_INDEX, &joint3_config);

  RARM_SimpleConfig_t joint2_config = {.OVERCURRENT_SD = OC_NOSHUTDOWN,
                                       .VSCOMP = VSCOMP_DISABLE,
                                       .STEP_MODE = SM_128_MICROSTEP,
                                       .steps_rev = 200,
                                       .reduction_ratio = 50,
                                       .supply_voltage = 24.0f,
                                       .run_acc_dec_voltage = 1.8f,
                                       .hold_voltage = 1.5f,
                                       .max_speed = 800,
                                       .min_speed = 0,
                                       .acceleration = 800,
                                       .deceleration = 800,
                                       .fullstep_speed = 800,
                                       .oc_threshold = 20.0f,
                                       .stall_threshold = 20.0f,
                                       .min_degs = -180,
                                       .max_degs = 180,
                                       .st_slp = 0x19,
                                       .fn_slp_acc = 0x79,
                                       .fn_slp_dec = 0x79};
  RARM_SetConfig(J2_INDEX, &joint2_config);

  SHARED_DATA->motor_ready     = 1U;
  SHARED_DATA->motor_ready_seq = 1U;
  __DSB();

  uint32_t last_motor_tick = HAL_GetTick();
  while (1) {
    uint32_t now = HAL_GetTick();
    if ((now - last_motor_tick) >= MOTOR_LOOP_PERIOD_MS) {
      last_motor_tick = now;
#if WIRE_TEST
      static uint32_t test_state_timer = 0;
      static uint8_t  current_test_joint = 0;
      static uint8_t  test_direction = 0;

      if ((now - test_state_timer) >= 2000) {
        test_state_timer = now;
        if (test_direction == 0) {
          if (current_test_joint == J2_INDEX || current_test_joint == J3_INDEX) {
            RARM_ReleaseBrake(current_test_joint);
            DelayMs(BRAKE_SETTLE_MS);
          }
          RARM_MoveDegrees(current_test_joint, 10);
          test_direction = 1;
        } else if (test_direction == 1) {
          RARM_MoveDegrees(current_test_joint, -10);
          test_direction = 2;
        } else {
          if (current_test_joint == J2_INDEX || current_test_joint == J3_INDEX) {
            RARM_EngageBrake(current_test_joint);
          }
          current_test_joint = (current_test_joint + 1) % N_JOINTS;
          test_direction = 0;
        }
      }
#else
      Motor_ProcessCommands();
#endif
      Motor_UpdatePositions();

      HAL_HSEM_FastTake(HSEM_ID_SENSOR);
      HAL_HSEM_Release(HSEM_ID_SENSOR, 0);
    }
  }
}

#if !WIRE_TEST
static void Motor_ProcessCommands(void)
{
  uint32_t seq_a = SHARED_DATA->joint_cmd_seq;
  __DMB();
  uint32_t seq_b = SHARED_DATA->joint_cmd_seq;
  if (seq_a != seq_b) {
    return;
  }
  uint32_t current_seq = seq_a;

  if (current_seq == last_processed_cmd_seq) {
    uint32_t now_ms = HAL_GetTick();
    for (uint8_t i = 0; i < N_JOINTS; i++) {
      if (i != J2_INDEX && i != J3_INDEX) continue;
      if (!motion_pending[i]) continue;
      if (RARM_IsMoving(i)) {
        motion_pending_start_ms[i] = now_ms;
        continue;
      }
      if ((now_ms - motion_pending_start_ms[i]) >= BRAKE_SETTLE_MS) {
        RARM_EngageBrake(i);
        motion_pending[i] = false;
      }
    }
    return;
  }

  for (uint8_t i = 0; i < N_JOINTS; i++) {
    int32_t cmd_mdeg     = SHARED_DATA->joint_cmd_positions[i];
    int32_t current_mdeg = RARM_GetPositionMilliDegrees(i);
    int32_t delta_mdeg   = cmd_mdeg - current_mdeg;

    if (delta_mdeg != 0) {
      if (i == J2_INDEX || i == J3_INDEX) {
        RARM_ReleaseBrake(i);
        motion_pending[i] = true;
        motion_pending_start_ms[i] = HAL_GetTick();
      }
      RARM_MoveMilliDegrees(i, delta_mdeg);
    }
  }

  last_processed_cmd_seq        = current_seq;
  SHARED_DATA->joint_cmd_ack    = current_seq;
  __DSB();
}
#endif

static void Motor_UpdatePositions(void)
{
  for (uint8_t i = 0; i < N_JOINTS; i++) {
    SHARED_DATA->joint_act_positions[i] = RARM_GetPositionMilliDegrees(i);
  }
  SHARED_DATA->motion_done_seq++;
  __DSB();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {
    LED_RED_ON();
    DelayMs(100);
    LED_RED_OFF();
    DelayMs(100);
  }
}
