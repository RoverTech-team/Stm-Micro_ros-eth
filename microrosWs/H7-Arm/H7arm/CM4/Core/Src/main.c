/**
 ******************************************************************************
 * @file           : main_debug.c
 * @brief          : JSN-SR04T ultrasonic sensor demo
 *
 * Sequence:
 *   PHASE 1 (5s): TRIG toggles 500ms on/500ms off  → probe PD1 with multimeter
 *   PHASE 2 (5s): raw ECHO read → GREEN=HIGH, RED=LOW
 *   PHASE 3 (5s): full trigger+echo attempt → GREEN=echo received, RED=timeout
 *   PHASE 4:      loop showing distance via blinks (if phase 3 worked)
 ******************************************************************************
 */

#include "main.h"
#include "shared_data.h"
#include <stdio.h> // Indispensabile per la funzione printf

TIM_HandleTypeDef htim2;
/* Assicurati che la variabile UART globale generata da CubeMX sia visibile */
/* Sostituisci huart1 con l'handle della UART che hai scelto (es. huart3) */
UART_HandleTypeDef huart4;

#define TIMER_TARGET_HZ 1000000U
#define TRIG_RESET_TIME_US 2U
#define TRIG_PULSE_TIME_US 10U
#define ECHO_WAIT_TIMEOUT_US 30000U
#define ECHO_PULSE_TIMEOUT_US 26000U

#define BUSY(n)                                                                \
  do {                                                                         \
    for (volatile int _i = 0; _i < (n); _i++)                                  \
      ;                                                                        \
  } while (0)

#define LED_GREEN_ON() (GPIOB->BSRR = (1U << 0))
#define LED_GREEN_OFF() (GPIOB->BSRR = (1U << 16))
#define LED_RED_ON() (GPIOB->BSRR = (1U << 14))
#define LED_RED_OFF() (GPIOB->BSRR = (1U << 30))
#define LED_ALL_OFF()                                                          \
  do {                                                                         \
    LED_GREEN_OFF();                                                           \
    LED_RED_OFF();                                                             \
  } while (0)

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_UART4_Init(void);

int main(void) {
  /* ------------------------------------------------------------------ */
  /* Pre-HAL GPIO: enable GPIOB for LEDs                                */
  /* ------------------------------------------------------------------ */
  RCC->AHB4ENR |= (1U << 1);
  BUSY(10000);
  GPIOB->MODER &= ~(3U << 0);
  GPIOB->MODER |= (1U << 0); /* PB0  out */
  GPIOB->MODER &= ~(3U << 28);
  GPIOB->MODER |= (1U << 28); /* PB14 out */

  /* Quick alive blink */
  for (int j = 0; j < 2; j++) {
    LED_GREEN_ON();
    LED_RED_ON();
    BUSY(3000000);
    LED_ALL_OFF();
    BUSY(3000000);
  }

  HAL_Init();
  SystemClock_Config();
  SCB->VTOR = 0x08100000;
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_UART4_Init();
  /* Abilitazione del clock per gli Hardware Semaphores */
  __HAL_RCC_HSEM_CLK_ENABLE();

  /* Compute prescaler */
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  uint32_t apb1pre =
      (RCC->D2CFGR & RCC_D2CFGR_D2PPRE1_Msk) >> RCC_D2CFGR_D2PPRE1_Pos;
  uint32_t tim2clk = (apb1pre >= 0x04U) ? (pclk1 * 2U) : pclk1;
  uint32_t psc = (tim2clk / TIMER_TARGET_HZ);
  if (psc > 0U) {
    psc -= 1U;
  }
  __HAL_TIM_SET_PRESCALER(&htim2, psc);
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  HAL_TIM_GenerateEvent(&htim2, TIM_EVENTSOURCE_UPDATE);
  HAL_TIM_Base_Start(&htim2);

  /* Clocks OK — 2 green blinks */
  for (int j = 0; j < 2; j++) {
    LED_GREEN_ON();
    HAL_Delay(200U);
    LED_GREEN_OFF();
    HAL_Delay(200U);
  }
  HAL_Delay(500U);

  /* ================================================================== */
  /* PHASE 1: TRIG toggle — probe PD1 with multimeter                   */
  /* Expected: ~3.3V / 0V alternating every 500ms                       */
  /* Failure:  stuck at 0V → GPIOD clock or MX_GPIO_Init broken         */
  /* ================================================================== */

  /* 1× slow red = entering phase 1 */
  LED_RED_ON();
  HAL_Delay(1000U);
  LED_RED_OFF();
  HAL_Delay(500U);

  uint32_t phase1_start = HAL_GetTick();
  while (HAL_GetTick() - phase1_start < 5000U) {
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);
    LED_GREEN_ON();
    HAL_Delay(500U);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
    LED_GREEN_OFF();
    HAL_Delay(500U);
  }
  HAL_Delay(1000U);

  /* ================================================================== */
  /* PHASE 2: Raw ECHO read — no trigger sent                           */
  /* GREEN = PD0 reads HIGH                                             */
  /* RED   = PD0 reads LOW  (expected with PULLDOWN and no signal)      */
  /* ================================================================== */

  /* 2× slow red = entering phase 2 */
  for (int j = 0; j < 2; j++) {
    LED_RED_ON();
    HAL_Delay(500U);
    LED_RED_OFF();
    HAL_Delay(500U);
  }
  HAL_Delay(500U);

  uint32_t phase2_start = HAL_GetTick();
  while (HAL_GetTick() - phase2_start < 5000U) {
    if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0) == GPIO_PIN_SET) {
      LED_GREEN_ON();
      LED_RED_OFF();
    } else {
      LED_GREEN_OFF();
      LED_RED_ON();
    }
    HAL_Delay(50U);
  }
  LED_ALL_OFF();
  HAL_Delay(5000U);

  /* ================================================================== */
  /* PHASE 3: Full trigger → echo attempt, 10 tries                    */
  /* GREEN blink = echo received                                        */
  /* RED blink   = timeout                                              */
  /* ================================================================== */

  /* 3× slow red = entering phase 3 */
  for (int j = 0; j < 3; j++) {
    LED_RED_ON();
    HAL_Delay(500U);
    LED_RED_OFF();
    HAL_Delay(500U);
  }
  HAL_Delay(500U);

  uint8_t any_echo = 0U;
  uint32_t echo_time = 0U;

  for (int attempt = 0; attempt < 10; attempt++) {
    /* Trigger */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < TRIG_RESET_TIME_US)
      ;

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < TRIG_PULSE_TIME_US)
      ;
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);

    /* Wait for echo HIGH */
    uint8_t got_echo = 0U;
    uint32_t wait_start = __HAL_TIM_GET_COUNTER(&htim2);
    while (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0) == GPIO_PIN_RESET) {
      if ((uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - wait_start) >
          ECHO_WAIT_TIMEOUT_US) {
        break;
      }
    }

    if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0) == GPIO_PIN_SET) {
      uint32_t echo_start = __HAL_TIM_GET_COUNTER(&htim2);
      while (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0) == GPIO_PIN_SET) {
        if ((uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - echo_start) >
            ECHO_PULSE_TIMEOUT_US) {
          break;
        }
      }
      echo_time = (uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - echo_start);
      got_echo = 1U;
      any_echo = 1U;
    }

    if (got_echo) {
      /* GREEN blink = echo received this attempt */
      LED_GREEN_ON();
      HAL_Delay(300U);
      LED_GREEN_OFF();
    } else {
      /* RED blink = no echo this attempt */
      LED_RED_ON();
      HAL_Delay(300U);
      LED_RED_OFF();
    }
    HAL_Delay(200U);
  }
  HAL_Delay(1000U);

  /* ================================================================== */
  /* PHASE 4: Result                                                     */
  /* If any echo was received: distance blinks on green                 */
  /* If no echo at all:        solid red forever → debug wiring         */
  /* ================================================================== */
  if (!any_echo) {
    /* Solid red = complete failure, check wiring */
    LED_RED_ON();
    while (1)
      ;
  }

  /* Distance in cm via green blinks (up to 9, then pause) */
  uint32_t dist_cm = echo_time / 58U;

  /* ── Scrivi prima misura nella shared memory ── */
  SHARED_DATA->distance_cm = dist_cm;
  SHARED_DATA->data_ready = 1U;
  __DSB(); /* Forza la scrittura a raggiungere SRAM4 */
  HAL_HSEM_FastTake(HSEM_ID_SENSOR);
  HAL_HSEM_Release(HSEM_ID_SENSOR, 0);

  while (1) {
    printf("Misura rilevata: %lu cm\r\n", dist_cm);
    HAL_Delay(500U); /* 500ms pause between updates */

    /* Retrigger for fresh distance */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < TRIG_RESET_TIME_US)
      ;
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < TRIG_PULSE_TIME_US)
      ;
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);

    uint32_t ws = __HAL_TIM_GET_COUNTER(&htim2);
    while (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0) == GPIO_PIN_RESET) {
      if ((uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - ws) >
          ECHO_WAIT_TIMEOUT_US) {
        break;
      }
    }
    if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0) == GPIO_PIN_SET) {
      uint32_t es = __HAL_TIM_GET_COUNTER(&htim2);
      while (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0) == GPIO_PIN_SET) {
        if ((uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - es) >
            ECHO_PULSE_TIMEOUT_US) {
          break;
        }
      }
      echo_time = (uint32_t)(__HAL_TIM_GET_COUNTER(&htim2) - es);
      dist_cm = echo_time / 58U;
    }

    /* ── Aggiorna shared memory SEMPRE (anche se dist_cm invariato) ── */
    SHARED_DATA->distance_cm = dist_cm;
    SHARED_DATA->data_ready = 1U;
    __DSB(); /* Forza la scrittura a raggiungere SRAM4 */
  }
}
static void MX_UART4_Init(void) {

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart4, UART_TXFIFO_THRESHOLD_1_8) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart4, UART_RXFIFO_THRESHOLD_1_8) !=
      HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart4) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */
}

/* ========================================================================
 * Peripherals
 * ======================================================================== */
static void SystemClock_Config(void) {
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
  RCC_ClkInitStruct.APB1CLKDivider =
      RCC_APB1_DIV4; /* DEVE essere uguale al CM7! */
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_TIM2_Init(void) {
  __HAL_RCC_TIM2_CLK_ENABLE();

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 0xFFFFFFFFU;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_14, GPIO_PIN_RESET);

  /* PD0 — ECHO input */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* PD1 — TRIG output */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* PB0 — green LED */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PB14 — red LED */
  GPIO_InitStruct.Pin = GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void) {
  __disable_irq();
  while (1) {
    LED_RED_ON();
    BUSY(500000);
    LED_RED_OFF();
    BUSY(500000);
  }
}

int _write(int file, char *ptr, int len) {
  /* La funzione HAL_UART_Transmit è un algoritmo bloccante O(n).
   * Sospenderà l'esecuzione del core finché l'ultimo bit dell'intero
   * buffer non sarà fisicamente uscito dal pin TX.
   * Parametri: l'handle della UART, il puntatore ai dati, la lunghezza,
   * e un timeout di sicurezza (es. 1000 millisecondi).
   */
  HAL_UART_Transmit(&huart4, (uint8_t *)ptr, len, 1000);
  return len;
}