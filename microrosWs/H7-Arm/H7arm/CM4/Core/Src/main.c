/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : CM4 bare-metal firmware — motor driver + ultrasonic sensor
 *
 * This firmware runs on the Cortex-M4 core of the STM32H755.
 * It reads joint commands from shared SRAM4 (written by CM7) and
 * drives the powerSTEP01 stepper motors via SPI daisy-chain.
 * Actual joint positions are written back to shared SRAM4 for
 * CM7 to publish as /joint_states.
 *
 * The ultrasonic sensor loop is preserved alongside motor control.
 ******************************************************************************
 */

#include "main.h"
#include "drivers/RArm/rarm.h"
#include "drivers/powerSTEP01/ps01.h"
#include "shared_data.h"
#include <string.h>

/* ---- Timer / delay configuration ---- */
#define TIMER_PRESCALER_1MHZ 223U
#define TRIG_RESET_TIME_US 2U
#define TRIG_PULSE_TIME_US 10U
#define ECHO_WAIT_TIMEOUT_US 30000U
#define ECHO_PULSE_TIMEOUT_US 60000U
#define SENSOR_MAX_DISTANCE_CM 600U
#define SENSOR_STARTUP_SETTLE_MS 300U
/* Enable wire test mode: cycles through moving all joints one by one */
#define WIRE_TEST 1

/* Motor control loop period (ms) */
#define MOTOR_LOOP_PERIOD_MS 10U

/* ---- Busy-wait macro for startup ---- */
#define BUSY(n)                                                                \
  do {                                                                         \
    for (volatile int _i = 0; _i < (n); _i++)                                  \
      ;                                                                        \
  } while (0)

/* ---- LED macros ---- */
#define LED_GREEN_ON() (GPIOB->BSRR = GPIO_PIN_0)
#define LED_GREEN_OFF() (GPIOB->BSRR = (GPIO_PIN_0 << 16))
#define LED_RED_ON() (GPIOB->BSRR = GPIO_PIN_14)
#define LED_RED_OFF() (GPIOB->BSRR = (GPIO_PIN_14 << 16))

/* ---- Ultrasonic trigger/echo ---- */
#define TRIG_HIGH() (GPIOD->BSRR = GPIO_PIN_1)
#define TRIG_LOW() (GPIOD->BSRR = (GPIO_PIN_1 << 16))
#define ECHO_IS_HIGH() (((GPIOD->IDR & GPIO_PIN_0) != 0U) ? 1U : 0U)

/* ---- SPI handle (used by powerSTEP01 driver) ---- */
SPI_HandleTypeDef hspi1;

/* ---- Bare-metal OS interface (from ps01_baremetal_os.c) ---- */
extern const PS01_OS_t *ps01_baremetal_get_os(void);

/* ---- Motor bank ---- */
static Stepper_t motors[N_JOINTS];
static StepperBank_t bank = {
    .motors = motors,
    .active = 0,
};

/* ---- Forward declarations ---- */
static uint32_t Timer2_NowUs(void);
static void DelayMs(uint32_t delay_ms);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void Timer2_Init(void);
static uint8_t Sensor_Measure(uint32_t *distance_cm);
#ifndef WIRE_TEST
static void Motor_ProcessCommands(void);
#endif
static void Motor_UpdatePositions(void);

/* ==================================================================== */
/*  Timer helpers                                                       */
/* ==================================================================== */

static uint32_t Timer2_NowUs(void) { return TIM2->CNT; }

static void DelayMs(uint32_t delay_ms) {
  uint32_t start_us = Timer2_NowUs();
  uint32_t delay_us = delay_ms * 1000U;
  while ((uint32_t)(Timer2_NowUs() - start_us) < delay_us)
    ;
}

/* ==================================================================== */
/*  Ultrasonic sensor (unchanged from original)                         */
/* ==================================================================== */

static uint8_t Sensor_Measure(uint32_t *distance_cm) {
  uint8_t got_echo = 0U;
  uint8_t wait_timeout = 0U;
  uint8_t pulse_timeout = 0U;
  uint32_t echo_ticks = 0U;
  uint32_t measured_distance_cm = *distance_cm;

  SHARED_DATA->cm4_write_seq += 1U;
  SHARED_DATA->cm4_last_echo_ok = 0U;
  SHARED_DATA->cm4_last_echo_ticks = 0U;
  SHARED_DATA->cm4_last_wait_timeout = 0U;
  SHARED_DATA->cm4_last_pulse_timeout = 0U;
  SHARED_DATA->cm4_last_measurement_valid = 0U;
  SHARED_DATA->data_ready = 0U;
  __DSB();

  TRIG_LOW();
  TIM2->CNT = 0U;
  while (Timer2_NowUs() < TRIG_RESET_TIME_US)
    ;

  TRIG_HIGH();
  TIM2->CNT = 0U;
  while (Timer2_NowUs() < TRIG_PULSE_TIME_US)
    ;
  TRIG_LOW();

  uint32_t wait_start = Timer2_NowUs();
  while (!ECHO_IS_HIGH()) {
    if ((uint32_t)(Timer2_NowUs() - wait_start) > ECHO_WAIT_TIMEOUT_US) {
      wait_timeout = 1U;
      break;
    }
  }

  if (!wait_timeout && ECHO_IS_HIGH()) {
    uint32_t echo_start = Timer2_NowUs();
    while (ECHO_IS_HIGH()) {
      if ((uint32_t)(Timer2_NowUs() - echo_start) > ECHO_PULSE_TIMEOUT_US) {
        pulse_timeout = 1U;
        break;
      }
    }
    echo_ticks = (uint32_t)(Timer2_NowUs() - echo_start);
    if (!pulse_timeout) {
      measured_distance_cm = echo_ticks / 58U;
      got_echo = 1U;
    } else {
      measured_distance_cm = SENSOR_MAX_DISTANCE_CM;
      got_echo = 1U;
    }
  }

  SHARED_DATA->cm4_last_echo_ok = got_echo;
  SHARED_DATA->cm4_last_echo_ticks = echo_ticks;
  SHARED_DATA->cm4_last_wait_timeout = wait_timeout;
  SHARED_DATA->cm4_last_pulse_timeout = pulse_timeout;
  SHARED_DATA->cm4_last_measurement_valid = got_echo;

  if (got_echo) {
    *distance_cm = measured_distance_cm;
    SHARED_DATA->distance_cm = measured_distance_cm;
    SHARED_DATA->data_ready = 1U;
  }

  __DSB();
  return got_echo;
}

/* ==================================================================== */
/*  Motor control                                                       */
/* ==================================================================== */

#ifndef WIRE_TEST
/** Track last processed command sequence to detect new commands */
static uint32_t last_processed_cmd_seq = 0U;

/**
 * Check if CM7 has written a new joint command.
 * If so, drive motors to the commanded positions.
 */
static void Motor_ProcessCommands(void) {
  uint32_t current_seq = SHARED_DATA->joint_cmd_seq;

  if (current_seq == last_processed_cmd_seq) {
    /* No new command, check if moving joints J2 and J3 have reached their
     * targets to engage brakes */
    for (uint8_t i = 0; i < N_JOINTS; i++) {
      if (i == J2_INDEX || i == J3_INDEX) {
        float cmd_deg = SHARED_DATA->joint_cmd_positions[i];
        float actual_deg = SHARED_DATA->joint_act_positions[i];
        int16_t diff = (int16_t)(cmd_deg - actual_deg);
        if (diff == 0) {
          RARM_EngageBrake(i);
        }
      }
    }
    return;
  }

  /* Process new command — move each joint to commanded position */
  for (uint8_t i = 0; i < N_JOINTS; i++) {
    float cmd_deg = SHARED_DATA->joint_cmd_positions[i];
    int32_t current_deg = RARM_GetPositionDegrees(i);
    int16_t delta = (int16_t)(cmd_deg - (float)current_deg);

    if (delta != 0) {
      if (i == J2_INDEX || i == J3_INDEX) {
        RARM_ReleaseBrake(i);
      }
      RARM_MoveDegrees(i, delta);
    }
  }

  /* Acknowledge the command */
  last_processed_cmd_seq = current_seq;
  SHARED_DATA->joint_cmd_ack = current_seq;
  __DSB();
}
#endif

/**
 * Read actual positions from all motors and write to shared memory.
 */
static void Motor_UpdatePositions(void) {
  for (uint8_t i = 0; i < N_JOINTS; i++) {
    SHARED_DATA->joint_act_positions[i] = (float)RARM_GetPositionDegrees(i);
  }
  __DSB();
}

/* ==================================================================== */
/*  Peripheral initialization                                          */
/* ==================================================================== */

/**
 * Initialize GPIO for LEDs and ultrasonic sensor.
 */
static void MX_GPIO_Init(void) {
  /* Enable GPIO clocks */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /* PB0 (green LED) and PB14 (red LED) as output */
  GPIOB->MODER &= ~((3U << (0 * 2U)) | (3U << (14 * 2U)));
  GPIOB->MODER |= (1U << (0 * 2U)) | (1U << (14 * 2U));

  /* PD0 (echo, input) and PD1 (trigger, output) */
  GPIOD->MODER &= ~((3U << (0 * 2U)) | (3U << (1 * 2U)));
  GPIOD->MODER |= (1U << (1 * 2U));
  GPIOD->PUPDR &= ~((3U << (0 * 2U)) | (3U << (1 * 2U)));
  GPIOD->PUPDR |= (2U << (0 * 2U));
  GPIOD->OTYPER &= ~(1U << 1);
  GPIOD->OSPEEDR &= ~((3U << (0 * 2U)) | (3U << (1 * 2U)));

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Configure CS pin (PD14): Output Push-Pull, High Speed, starting HIGH */
  GPIO_InitStruct.Pin = DRV_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DRV_CS_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);

  /* Configure Reset pin (PG9): Output Push-Pull, High Speed, starting LOW */
  GPIO_InitStruct.Pin = DRV_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DRV_RESET_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(DRV_RESET_GPIO_Port, DRV_RESET_Pin, GPIO_PIN_RESET);

  /* Configure J2 Brake pin (PD15): Output Push-Pull, Low Speed, starting LOW */
  GPIO_InitStruct.Pin = J2_BRAKE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(J2_BRAKE_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(J2_BRAKE_GPIO_Port, J2_BRAKE_Pin, GPIO_PIN_RESET);

  /* Configure J3 Brake pin (PA8): Output Push-Pull, Low Speed, starting LOW */
  GPIO_InitStruct.Pin = J3_BRAKE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(J3_BRAKE_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(J3_BRAKE_GPIO_Port, J3_BRAKE_Pin, GPIO_PIN_RESET);
}

/**
 * Initialize SPI1 for powerSTEP01 communication.
 * PowerSTEP01 requires: CPOL=1, CPHA=1 (SPI Mode 3), MSB first.
 *
 * TODO: Verify SPI pins match hardware wiring.
 * Default SPI1 pins on NUCLEO-H755ZI-Q:
 *   PA5 = SCK, PA6 = MISO, PA7 = MOSI
 */
static void MX_SPI1_Init(void) {
  __HAL_RCC_SPI1_CLK_ENABLE();

  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH; /* CPOL = 1 */
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;      /* CPHA = 1 */
  hspi1.Init.NSS = SPI_NSS_SOFT;              /* CS managed manually */
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;

  if (HAL_SPI_Init(&hspi1) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * Initialize TIM2 as a 1 MHz free-running counter for microsecond timing.
 */
static void Timer2_Init(void) {
  RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;
  TIM2->CR1 = 0U;
  TIM2->PSC = TIMER_PRESCALER_1MHZ;
  TIM2->ARR = 0xFFFFFFFFU;
  TIM2->CNT = 0U;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CR1 = TIM_CR1_CEN;
}

/* ==================================================================== */
/*  main()                                                              */
/* ==================================================================== */

int main(void) {
  /* ---- HAL init (needed for HAL_Delay, SPI, DMA) ---- */
  HAL_Init();

  /* ---- Startup LED blink ---- */
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
  BUSY(10000);
  GPIOB->MODER &= ~((3U << (0 * 2U)) | (3U << (14 * 2U)));
  GPIOB->MODER |= (1U << (0 * 2U)) | (1U << (14 * 2U));

  for (int j = 0; j < 2; j++) {
    LED_GREEN_ON();
    LED_RED_ON();
    BUSY(3000000);
    LED_GREEN_OFF();
    LED_RED_OFF();
    BUSY(3000000);
  }

  /* ---- Vector table for CM4 ---- */
  SCB->VTOR = 0x08100000;

  /* ---- Peripheral init ---- */
  MX_GPIO_Init();
  Timer2_Init();
  MX_SPI1_Init();

  /* ---- Clear shared memory ---- */
  memset((void *)SHARED_DATA, 0, sizeof(*SHARED_DATA));
  SHARED_DATA->cm4_write_seq = 1U;
  __DSB();

  /* ---- Ultrasonic settle ---- */
  TRIG_LOW();
  DelayMs(SENSOR_STARTUP_SETTLE_MS);

  /* ---- Reset powerSTEP01 drivers (Active Low Reset) ---- */
  HAL_GPIO_WritePin(DRV_RESET_GPIO_Port, DRV_RESET_Pin, GPIO_PIN_RESET);
  DelayMs(10);
  HAL_GPIO_WritePin(DRV_RESET_GPIO_Port, DRV_RESET_Pin, GPIO_PIN_SET);
  DelayMs(10);

  /* ---- Initialize powerSTEP01 motor driver ---- */
  RARM_SetBank(&bank);
  ps01Init(ps01_baremetal_get_os());

  /* ---- Configure each motor with actual parameters from electronics team ----
   */
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
  /* Signal CM7 that motor driver is ready */
  SHARED_DATA->motor_ready = 1U;
  __DSB();

  LED_GREEN_ON();
  LED_RED_OFF();

  /* ---- Main loop ---- */
  uint32_t dist_cm = 100U;
  uint32_t last_motor_tick = HAL_GetTick();

  while (1) {
    /* --- Ultrasonic sensor measurement --- */

    /* --- Motor control (every MOTOR_LOOP_PERIOD_MS) --- */
    uint32_t now = HAL_GetTick();
    if ((now - last_motor_tick) >= MOTOR_LOOP_PERIOD_MS) {
      last_motor_tick = now;
#ifdef WIRE_TEST
      /* --- Wire Test Logic --- */
      static uint32_t test_state_timer = 0;
      static uint8_t current_test_joint = 0;
      static uint8_t test_direction =
          0; // 0 = move +10, 1 = move -10, 2 = pause/next

      if ((now - test_state_timer) >= 2000) {
        test_state_timer = now;
        if (test_direction == 0) {
          /* Release brake if J2 or J3 */
          if (current_test_joint == J2_INDEX ||
              current_test_joint == J3_INDEX) {
            RARM_ReleaseBrake(current_test_joint);
            DelayMs(100); /* Allow brake to release */
          }
          RARM_MoveDegrees(current_test_joint, 10);
          test_direction = 1;
        } else if (test_direction == 1) {
          /* Move back to 0 */
          RARM_MoveDegrees(current_test_joint, -10);
          test_direction = 2;
        } else {
          /* Engage brake if J2 or J3 and advance to next joint */
          if (current_test_joint == J2_INDEX ||
              current_test_joint == J3_INDEX) {
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
    }
  }
}

/* ==================================================================== */
/*  Error handler                                                       */
/* ==================================================================== */

void Error_Handler(void) {
  __disable_irq();
  while (1) {
    LED_RED_ON();
    BUSY(500000);
    LED_RED_OFF();
    BUSY(500000);
  }
}
