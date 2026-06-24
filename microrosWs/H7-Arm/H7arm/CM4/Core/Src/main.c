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

static void SystemClock_Config(void);

#ifndef WIRE_TEST
#define WIRE_TEST 1
#endif

#define MOTOR_LOOP_PERIOD_MS 10U
#define BRAKE_SETTLE_MS      100U

__attribute__((section(".shared"))) shared_data_t shared_data_inst;

extern const PS01_OS_t *ps01_baremetal_get_os(void);

static Stepper_t     motors[N_JOINTS];
static StepperBank_t bank = { .motors = motors, .active = 0 };

/* Round-robin fault blink state */
static uint8_t  rr_faults[N_JOINTS];
static uint8_t  rr_joint    = 0xFF;
static uint8_t  rr_blinks   = 0;
static uint8_t  rr_total    = 0;
static uint8_t  rr_phase    = 0;
static uint8_t  rr_pause    = 0;
static uint32_t rr_timer    = 0;

/* Button edge-detection state */
static uint8_t  btn_prev   = 1U;       /* pull-up, unpressed = HIGH */
static uint32_t btn_last_ms = 0;

/* Non-blocking green flash state (button "no fault" indicator) */
static uint8_t  btn_green_blinks = 0;
static uint8_t  btn_green_phase  = 0;
static uint32_t btn_green_timer  = 0;

/* Joint configuration array — indexed by J*_INDEX */
static RARM_SimpleConfig_t joint_configs[N_JOINTS] = {
    [J1_INDEX] = { .OVERCURRENT_SD = OC_NOSHUTDOWN,
                   .VSCOMP          = VSCOMP_DISABLE,
                   .STEP_MODE       = SM_128_MICROSTEP,
                   .steps_rev       = 200,
                   .reduction_ratio = 3,
                   .supply_voltage  = 24.0f,
                   .run_acc_dec_voltage = 3.5f,
                   .hold_voltage    = 2.5f,
                   .max_speed       = 100,
                   .min_speed       = 0,
                   .acceleration    = 300,
                   .deceleration    = 300,
                   .fullstep_speed  = 10,
                    .oc_threshold    = 40.0f,
                   .stall_threshold = 20.0f,
                   .min_degs        = -180,
                   .max_degs        = 180,
                   .st_slp          = 0x21,
                   .fn_slp_acc      = 0x89,
                   .fn_slp_dec      = 0x89 },
    [J4_INDEX] = { .OVERCURRENT_SD = OC_NOSHUTDOWN,
                   .VSCOMP          = VSCOMP_DISABLE,
                   .STEP_MODE       = SM_128_MICROSTEP,
                   .steps_rev       = 200,
                   .reduction_ratio = 5,
                   .supply_voltage  = 24.0f,
                   .run_acc_dec_voltage = 2.34f,
                   .hold_voltage    = 1.5f,
                   .max_speed       = 400,
                   .min_speed       = 0,
                   .acceleration    = 400,
                   .deceleration    = 400,
                   .fullstep_speed  = 40,
                    .oc_threshold    = 40.0f,
                   .stall_threshold = 20.0f,
                   .min_degs        = -180,
                   .max_degs        = 180,
                   .st_slp          = 0x59,
                   .fn_slp_acc      = 0x29,
                   .fn_slp_dec      = 0x29 },
    [J6_INDEX] = { .OVERCURRENT_SD = OC_NOSHUTDOWN,
                   .VSCOMP          = VSCOMP_DISABLE,
                   .STEP_MODE       = SM_128_MICROSTEP,
                   .steps_rev       = 200,
                   .reduction_ratio = 50,
                   .supply_voltage  = 24.0f,
                   .run_acc_dec_voltage = 2.34f,
                   .hold_voltage    = 1.5f,
                   .max_speed       = 1000,
                   .min_speed       = 0,
                   .acceleration    = 4000,
                   .deceleration    = 4000,
                   .fullstep_speed  = 100,
                    .oc_threshold    = 40.0f,
                   .stall_threshold = 20.0f,
                   .min_degs        = -180,
                   .max_degs        = 180,
                   .st_slp          = 0x21,
                   .fn_slp_acc      = 0x89,
                   .fn_slp_dec      = 0x89 },
    [J5_INDEX] = { .OVERCURRENT_SD = OC_NOSHUTDOWN,
                   .VSCOMP          = VSCOMP_DISABLE,
                   .STEP_MODE       = SM_128_MICROSTEP,
                   .steps_rev       = 200,
                   .reduction_ratio = 5,
                   .supply_voltage  = 24.0f,
                   .run_acc_dec_voltage = 2.34f,
                   .hold_voltage    = 1.5f,
                   .max_speed       = 400,
                   .min_speed       = 0,
                   .acceleration    = 400,
                   .deceleration    = 400,
                   .fullstep_speed  = 40,
                    .oc_threshold    = 40.0f,
                   .stall_threshold = 20.0f,
                   .min_degs        = -180,
                   .max_degs        = 180,
                   .st_slp          = 0x59,
                   .fn_slp_acc      = 0x29,
                   .fn_slp_dec      = 0x29 },
    [J3_INDEX] = { .OVERCURRENT_SD = OC_NOSHUTDOWN,
                   .VSCOMP          = VSCOMP_DISABLE,
                   .STEP_MODE       = SM_128_MICROSTEP,
                   .steps_rev       = 200,
                   .reduction_ratio = 50,
                   .supply_voltage  = 24.0f,
                   .run_acc_dec_voltage = 3.5f,
                   .hold_voltage    = 3.5f,
                   .max_speed       = 1000,
                   .min_speed       = 0,
                   .acceleration    = 1000,
                   .deceleration    = 1000,
                   .fullstep_speed  = 100,
                    .oc_threshold    = 40.0f,
                   .stall_threshold = 20.0f,
                   .min_degs        = -180,
                   .max_degs        = 180,
                   .st_slp          = 0x89,
                   .fn_slp_acc      = 0x79,
                   .fn_slp_dec      = 0x79 },
    [J2_INDEX] = { .OVERCURRENT_SD = OC_NOSHUTDOWN,
                   .VSCOMP          = VSCOMP_DISABLE,
                   .STEP_MODE       = SM_128_MICROSTEP,
                   .steps_rev       = 200,
                   .reduction_ratio = 50,
                   .supply_voltage  = 24.0f,
                   .run_acc_dec_voltage = 1.8f,
                   .hold_voltage    = 1.5f,
                   .max_speed       = 800,
                   .min_speed       = 0,
                   .acceleration    = 800,
                   .deceleration    = 800,
                   .fullstep_speed  = 80,
                    .oc_threshold    = 40.0f,
                   .stall_threshold = 20.0f,
                   .min_degs        = -180,
                   .max_degs        = 180,
                   .st_slp          = 0x19,
                   .fn_slp_acc      = 0x79,
                   .fn_slp_dec      = 0x79 },
};

#if !WIRE_TEST
static uint32_t last_processed_cmd_seq = 0U;
static bool     motion_pending[N_JOINTS] = { false };
static uint32_t motion_pending_start_ms[N_JOINTS] = { 0U };

static void Motor_ProcessCommands(void);
#endif

#if WIRE_TEST
static uint32_t test_timer     = 0;
static uint8_t  test_idx       = 0;
#endif

static void Motor_UpdatePositions(void);
static void RR_Tick(uint32_t now);
static void RR_PollFaults(void);
static uint8_t RR_BlinkCount(uint16_t status);
static uint8_t RR_AlarmByte(uint16_t status);
static uint8_t RR_NextFaulted(uint8_t start);

int main(void)
{
  extern uint32_t g_pfnVectors;
  SCB->VTOR = (uint32_t)&g_pfnVectors;

  HAL_Init();

  SystemClock_Config();

  /* HAL_InitTick may fail if SystemCoreClock is stale.  Force SysTick
   * directly: 1 ms period = SystemCoreClock / 1000. */
  SysTick->LOAD  = (SystemCoreClock / 1000U) - 1UL;
  SysTick->VAL   = 0UL;
  SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk |
                   SysTick_CTRL_TICKINT_Msk  |
                   SysTick_CTRL_ENABLE_Msk;

  /* Enable DWT cycle counter (always available on Cortex-M4). */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

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

  LED_RED_ON();

  /* Reset sequence: flush daisy-chain shift registers (CS asserted),
   * then one clean 1 ms reset pulse, then 100 ms settling.
   * The 24 V rail is stable by this point (~2.5 s into boot). */
  {
    uint8_t flush[N_JOINTS];
    memset(flush, 0, sizeof(flush));
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, flush, N_JOINTS, 100);
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);
  }
  HAL_GPIO_WritePin(DRV_RESET_GPIO_Port, DRV_RESET_Pin, GPIO_PIN_RESET);
  DelayMs(1);
  HAL_GPIO_WritePin(DRV_RESET_GPIO_Port, DRV_RESET_Pin, GPIO_PIN_SET);
  DelayMs(100);
  LED_RED_OFF();
  LED_GREEN_ON();
  SHARED_DATA->last_fault_code = 10; /* before RARM_SetBank */
  RARM_SetBank(&bank);
  SHARED_DATA->last_fault_code = 20; /* before ps01Init */
  ps01Init(ps01_baremetal_get_os());
  SHARED_DATA->last_fault_code = 30; /* before RARM_SetConfig loop */

  for (uint8_t i = 0; i < N_JOINTS; i++) {
    RARM_SetConfig(i, &joint_configs[i]);
  }

  SHARED_DATA->last_fault_code = 40; /* after config, before fault clear */
  memset(rr_faults, 0, sizeof(rr_faults));
  SHARED_DATA->fault_alarm = 0U;
  SHARED_DATA->last_fault_code = 0U;
  SHARED_DATA->last_fault_tick = 0U;
  mot_bank->active = J1_INDEX;

  SHARED_DATA->last_fault_code = 50; /* before SPI test */
  /* Debug: single-byte SPI test (like F4 reference) */
  {
    uint8_t txb[6] = {0}, rxb[6] = {0};
    mot_bank->active = J1_INDEX;
    txb[MOT_NUMBER - 1U - mot_bank->active] = 0xD0;
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_RESET);
    SPI1_Transfer(txb, rxb, MOT_NUMBER);
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);
    SHARED_DATA->last_fault_code = 51; /* after first SPI test */
    SHARED_DATA->last_fault_cfb = rxb[MOT_NUMBER - 1U - mot_bank->active];
  }

  SHARED_DATA->last_fault_code = 99;
  SHARED_DATA->motor_ready     = 1U;
  SHARED_DATA->motor_ready_seq = 1U;
  __DSB();

  uint32_t const cyccnt_1ms = SystemCoreClock / 1000UL;
  uint32_t last_cyc = DWT->CYCCNT;
  while (1) {
    uint32_t now_cyc = DWT->CYCCNT;
    if ((now_cyc - last_cyc) >= (cyccnt_1ms * MOTOR_LOOP_PERIOD_MS)) {
      last_cyc = now_cyc;
      uint32_t now_ms = now_cyc / cyccnt_1ms;
#if WIRE_TEST
      /* Rock physical J2 (software idx 5) ±10° every 2 s */
      RARM_ReleaseBrake(J2_INDEX);
      RARM_ReleaseBrake(J3_INDEX);
      if ((now_ms - test_timer) >= 2000) {
        test_timer = now_ms;
        if (test_idx == 0) {
          RARM_MoveDegrees(5, 10);
          test_idx = 1;
        } else {
          RARM_MoveDegrees(5, -10);
          test_idx = 0;
        }
      }
#else
      Motor_ProcessCommands();
#endif
      Motor_UpdatePositions();

      /* Button scan: falling edge (press with 50 ms debounce) */
      {
        uint8_t btn = HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin);
        if (btn_prev == 1U && btn == 0U && (now_ms - btn_last_ms) > 50U) {
          btn_last_ms = now_ms;
          uint8_t found = 0;
          for (uint8_t i = 0; i < N_JOINTS; i++) {
            mot_bank->active = i;
            uint16_t s = ps01GetStatus_chain();
            uint8_t alarm = RR_AlarmByte(s);
            SHARED_DATA->fault_alarm &= ~((uint32_t)0xFF << (i * 8U));
            if (alarm != 0U) {
              SHARED_DATA->fault_alarm |= (uint32_t)alarm << (i * 8U);
              rr_faults[i] = alarm;
              found = 1;
            } else {
              rr_faults[i] = 0U;
            }
          }
          if (found) {
            rr_joint = RR_NextFaulted(0);
            if (rr_joint < N_JOINTS) {
              rr_blinks = 0;
              rr_total  = RR_BlinkCount(rr_faults[rr_joint]);
              if (rr_total == 0) rr_total = 1;
              rr_phase  = 0;
              rr_pause  = 0;
              rr_timer  = now_ms;
              LED_RED_ON();
            }
          } else {
            btn_green_blinks = 3;
            btn_green_phase  = 0;
            btn_green_timer  = now_ms;
            LED_GREEN_ON();
          }
          /* Clear latched alarms so faulted joints can be re-commanded */
          for (uint8_t i = 0; i < N_JOINTS; i++) {
            mot_bank->active = i;
            ps01SetAlarms_chain(ALARM_STALL | ALARM_CMD_ERR | ALARM_OVC |
                                ALARM_THRM_SD | ALARM_THRM_WARN | ALARM_UVLO);
          }
        }
        btn_prev = btn;
      }
      RR_Tick(now_ms);
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
      if (rr_faults[i] != 0U) {
        continue;
      }
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
  /* only read configured joints (idx 0-2) */
  for (uint8_t i = 0; i < N_JOINTS; i++) {
    SHARED_DATA->joint_act_positions[i] = RARM_GetPositionMilliDegrees(i);
  }
  SHARED_DATA->motion_done_seq++;
  __DSB();
}

static void RR_PollFaults(void)
{
  uint32_t any_alarm = 0U;
  for (uint8_t i = 0; i < N_JOINTS; i++) {
    mot_bank->active = i;
    uint16_t status = ps01GetStatus_chain();
    SHARED_DATA->dbg_joint_status[i] = status;
    uint8_t alarm = RR_AlarmByte(status);
    SHARED_DATA->fault_alarm &= ~((uint32_t)0xFF << (i * 8U));
    if (alarm != 0U) {
      any_alarm = 1U;
      rr_faults[i] = alarm;
      SHARED_DATA->fault_alarm |= (uint32_t)alarm << (i * 8U);
      ps01SetAlarms_chain(ALARM_STALL | ALARM_CMD_ERR | ALARM_OVC |
                          ALARM_THRM_SD | ALARM_THRM_WARN | ALARM_UVLO);
    } else {
      rr_faults[i] = 0U;
    }
  }
  if (any_alarm != 0U && SHARED_DATA->last_fault_code == 0U) {
    SHARED_DATA->last_fault_code = SHARED_DATA->fault_alarm;
    SHARED_DATA->last_fault_tick = HAL_GetTick();
  }
}

static uint8_t RR_BlinkCount(uint16_t status)
{
  PS01Status_t s = { .reg = status };
  if (s.bits.OCD)                 return 1;
  if (s.bits.TH_STATUS != 0U)     return 2;
  if (s.bits.UVLO || s.bits.UVLO_ADC) return 3;
  if (s.bits.STALL_B || s.bits.STALL_A) return 4;
  if (s.bits.SW_EVN)              return 5;
  if (s.bits.CMD_ERROR)           return 6;
  return 0;
}

static uint8_t RR_AlarmByte(uint16_t status)
{
  PS01Status_t s = { .reg = status };
  uint8_t alarm = 0;
  if (s.bits.OCD)            alarm |= ALARM_OVC;
  if (s.bits.TH_STATUS == 2) alarm |= ALARM_THRM_SD;
  if (s.bits.TH_STATUS == 1) alarm |= ALARM_THRM_WARN;
  if (s.bits.UVLO)           alarm |= ALARM_UVLO;
  if (s.bits.UVLO_ADC)       alarm |= ALARM_ADC_UVLO;
  if (s.bits.STALL_B || s.bits.STALL_A) alarm |= ALARM_STALL;
  if (s.bits.SW_EVN)         alarm |= ALARM_SW_EVENT;
  if (s.bits.CMD_ERROR)      alarm |= ALARM_CMD_ERR;
  return alarm;
}

static uint8_t RR_NextFaulted(uint8_t start)
{
  for (uint8_t i = 0; i < N_JOINTS; i++) {
    uint8_t idx = (start + i) % N_JOINTS;
    if (rr_faults[idx] != 0U) return idx;
  }
  return 0xFF;
}

static void RR_Tick(uint32_t now)
{
  RR_PollFaults();

  /* Green flash state machine (100ms timing, independent of red blink) */
  if (btn_green_blinks > 0) {
    if ((now - btn_green_timer) >= 100U) {
      btn_green_timer = now;
      if (btn_green_phase == 0) {
        LED_GREEN_OFF();
        btn_green_phase = 1;
      } else {
        LED_GREEN_ON();
        if (--btn_green_blinks == 0) {
          LED_GREEN_OFF();
          btn_green_phase = 0;
        } else {
          btn_green_phase = 0;
        }
      }
    }
    return;  /* suppress red blink during green flash */
  }

  if ((now - rr_timer) < 120U) return;
  rr_timer = now;

  if (rr_joint >= N_JOINTS || rr_faults[rr_joint] == 0U) {
    rr_joint = RR_NextFaulted(rr_joint + 1);
    if (rr_joint >= N_JOINTS) { LED_RED_OFF(); return; }
    rr_blinks = 0;
    rr_total  = RR_BlinkCount(rr_faults[rr_joint]);
    if (rr_total == 0) rr_total = 1;
    rr_phase  = 0;
    rr_pause  = 0;
    LED_RED_ON();
    return;
  }

  switch (rr_phase) {
  case 0:
    LED_RED_OFF();
    rr_phase = 1;
    break;

  case 1:
    rr_blinks++;
    if (rr_blinks >= rr_total) {
      rr_phase = 2;
      rr_pause = 5;
    } else {
      rr_phase = 0;
      LED_RED_ON();
    }
    break;

  case 2:
    if (--rr_pause == 0) {
      uint8_t next = RR_NextFaulted(rr_joint + 1);
      if (next >= N_JOINTS || next == rr_joint)
        next = RR_NextFaulted(0);
      rr_joint = (next >= N_JOINTS) ? 0xFF : next;
      rr_blinks = 0;
      rr_phase  = 0;
      rr_pause  = 0;
      if (rr_joint < N_JOINTS) {
        rr_total = RR_BlinkCount(rr_faults[rr_joint]);
        if (rr_total == 0) rr_total = 1;
        LED_RED_ON();
      }
    }
    break;
  }
}

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    ;

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 28;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 1024;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
    Error_Handler();
  }
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
