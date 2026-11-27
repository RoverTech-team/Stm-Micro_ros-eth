/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pump.c
  * @brief   Pump PWM control (MOSFET gate) for the CM7 micro-ROS firmware.
  *          Replaces the Arduino analogWrite()/map() logic from reference.c
  *          with native STM32H7 HAL timer PWM output.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "pump.h"
#include "main.h"

#include <stdbool.h>

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

static TIM_HandleTypeDef htim_pump;

static uint32_t Pump_GetTimerClockHz(void)
{
  /* TIM3 is on APB1 (D2 domain on the H7). The timer clock is the APB1
   * clock multiplied by 2 when the APB1 prescaler is not 1. */
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  uint32_t apb1_div = RCC->D2CFGR & RCC_D2CFGR_D2PPRE1;

  if(apb1_div == RCC_D2CFGR_D2PPRE1_DIV1)
  {
    return pclk1;
  }
  return pclk1 * 2U;
}

void Pump_Init(void)
{
  uint32_t timer_clock_hz = Pump_GetTimerClockHz();
  uint32_t period = (uint32_t)PUMP_PWM_PERIOD;
  uint32_t prescaler;
  GPIO_InitTypeDef gpio_init = {0};
  TIM_OC_InitTypeDef oc_init = {0};

  /* Enable the timer clock (generic handling) */
  switch((uint32_t)PUMP_TIMER_INSTANCE)
  {
    case (uint32_t)TIM1:  __HAL_RCC_TIM1_CLK_ENABLE();  break;
    case (uint32_t)TIM2:  __HAL_RCC_TIM2_CLK_ENABLE();  break;
    case (uint32_t)TIM3:  __HAL_RCC_TIM3_CLK_ENABLE();  break;
    case (uint32_t)TIM4:  __HAL_RCC_TIM4_CLK_ENABLE();  break;
    case (uint32_t)TIM5:  __HAL_RCC_TIM5_CLK_ENABLE();  break;
    case (uint32_t)TIM8:  __HAL_RCC_TIM8_CLK_ENABLE();  break;
    case (uint32_t)TIM12: __HAL_RCC_TIM12_CLK_ENABLE(); break;
    case (uint32_t)TIM13: __HAL_RCC_TIM13_CLK_ENABLE(); break;
    case (uint32_t)TIM14: __HAL_RCC_TIM14_CLK_ENABLE(); break;
    case (uint32_t)TIM15: __HAL_RCC_TIM15_CLK_ENABLE(); break;
    case (uint32_t)TIM16: __HAL_RCC_TIM16_CLK_ENABLE(); break;
    case (uint32_t)TIM17: __HAL_RCC_TIM17_CLK_ENABLE(); break;
    default: break;
  }

  if(PUMP_GPIO_PORT == GPIOA)      __HAL_RCC_GPIOA_CLK_ENABLE();
  else if(PUMP_GPIO_PORT == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
  else if(PUMP_GPIO_PORT == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
  else if(PUMP_GPIO_PORT == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
  else if(PUMP_GPIO_PORT == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
  else if(PUMP_GPIO_PORT == GPIOF) __HAL_RCC_GPIOF_CLK_ENABLE();
  else if(PUMP_GPIO_PORT == GPIOG) __HAL_RCC_GPIOG_CLK_ENABLE();
  else if(PUMP_GPIO_PORT == GPIOH) __HAL_RCC_GPIOH_CLK_ENABLE();
  else if(PUMP_GPIO_PORT == GPIOI) __HAL_RCC_GPIOI_CLK_ENABLE();

  /* Configure the PWM pin as alternate-function push-pull */
  gpio_init.Pin = PUMP_GPIO_PIN;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init.Alternate = PUMP_GPIO_AF;
  HAL_GPIO_Init(PUMP_GPIO_PORT, &gpio_init);

  /* Compute prescaler so the timer ticks at (frequency * (period + 1)).
   * Timer output freq = timer_clock / ((prescaler + 1) * (period + 1)). */
  if(timer_clock_hz == 0U || PUMP_PWM_FREQUENCY_HZ == 0U)
  {
    prescaler = 0U;
  }
  else
  {
    uint32_t ticks_per_period = timer_clock_hz / (PUMP_PWM_FREQUENCY_HZ * (period + 1U));
    if(ticks_per_period == 0U)
    {
      ticks_per_period = 1U;
    }
    prescaler = ticks_per_period - 1U;
  }

  htim_pump.Instance = PUMP_TIMER_INSTANCE;
  htim_pump.Init.Prescaler = (uint32_t)prescaler;
  htim_pump.Init.Period = period;
  htim_pump.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim_pump.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim_pump.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if(HAL_TIM_PWM_Init(&htim_pump) != HAL_OK)
  {
    Error_Handler();
  }

  oc_init.OCMode = TIM_OCMODE_PWM1;
  oc_init.Pulse = 0U;                       /* start at 0 % duty */
  oc_init.OCPolarity = TIM_OCPOLARITY_HIGH;
  oc_init.OCFastMode = TIM_OCFAST_DISABLE;
  if(HAL_TIM_PWM_ConfigChannel(&htim_pump, &oc_init, PUMP_PWM_CHANNEL) != HAL_OK)
  {
    Error_Handler();
  }

  if(HAL_TIM_PWM_Start(&htim_pump, PUMP_PWM_CHANNEL) != HAL_OK)
  {
    Error_Handler();
  }
}

void Pump_SetPercent(uint8_t percent)
{
  uint32_t compare;

  if(percent > 100U)
  {
    percent = 100U;
  }

  /* Map 0..100 % onto 0..PUMP_PWM_PERIOD (mimics Arduino map()). */
  compare = ((uint32_t)percent * (uint32_t)PUMP_PWM_PERIOD) / 100U;

  __HAL_TIM_SET_COMPARE(&htim_pump, PUMP_PWM_CHANNEL, compare);
}
