/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body for Cortex-M4 (Versione con Delay Manuale)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* --- VARIABILI GLOBALI --- */
TIM_HandleTypeDef htim2;

volatile uint32_t raw_time = 0;
volatile uint32_t distanza_cm = 0;

/* --- PROTOTIPI --- */
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
void delay_manuale(volatile uint32_t count); // Nuova funzione di ritardo

int main(void)
{
  /* Inizializzazione base */
  HAL_Init();
  MX_GPIO_Init();
  MX_TIM2_Init();


  uint32_t pclk1_freq = HAL_RCC_GetPCLK1Freq();
  if ((RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) != 0) { pclk1_freq *= 2; }
  
  if (pclk1_freq > 1000000) {
      uint32_t prescaler = (pclk1_freq / 1000000) - 1;
      __HAL_TIM_SET_PRESCALER(&htim2, prescaler);
  } else {
      __HAL_TIM_SET_PRESCALER(&htim2, 0);
  }
  HAL_TIM_Base_Start(&htim2);


  for(int i=0; i<6; i++) {
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0); // Inverte stato LED
      delay_manuale(200000); 
  }
  // Assicuriamoci che finisca SPENTO
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
  delay_manuale(500000); 


  while (1)
  {
    // 1. Reset Trigger
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while(__HAL_TIM_GET_COUNTER(&htim2) < 2);

    // 2. Impulso Trigger (10us)
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while(__HAL_TIM_GET_COUNTER(&htim2) < 10);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);

    // 3. Aspetta ECHO Alto
    uint32_t timeout = 1000000;
    while (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0) == GPIO_PIN_RESET) {
        if (timeout-- == 0) break;
    }

    // 4. Misura durata ECHO
    if (timeout > 0) {
        __HAL_TIM_SET_COUNTER(&htim2, 0);
        timeout = 1000000;
        while (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0) == GPIO_PIN_SET) {
            if (__HAL_TIM_GET_COUNTER(&htim2) > 50000) break; 
        }
        raw_time = __HAL_TIM_GET_COUNTER(&htim2);
        distanza_cm = raw_time / 58;
    } else {
        distanza_cm = 999; 
    }

    // 5. Logica LED: Acceso se < 10 cm
    //ignoriamo lo zero secco
    if (distanza_cm > 0 && distanza_cm < 10) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); 
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); 
    }

    // 6. Pausa tra le letture 
    delay_manuale(100000); 
  }
}


void delay_manuale(volatile uint32_t count)
{
    while(count--) {
        __asm("nop"); 
    }
}


static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0; 
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim2);

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig);

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  /* PD0 - ECHO */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN; 
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* PD1 - TRIG */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* PB0 - LED */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}