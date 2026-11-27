/*
 * Encoder GPIO init — 18× general FT pins, 5V-tolerant, no Timer/PWM
 * Docs: docs/pin-usage-encoder-proposal.md §3 v2
 * All pins INPUT pull-up (5V open-collector safe), VeryHigh speed for SSI.
 * Timers PWM untouched: TIM1/TIM2/TIM3/TIM4/TIM5/TIM8/TIM12/TIM15-17 remain free.
 */

#include "encoder_pins.h"
#include "main.h"

void MX_Encoder_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // Clocks for all ports used by the 18: C, D, E, F, G
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  // Common config: INPUT, PULLUP (5V OC safe), VeryHigh
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

  // --- CN10 I/O: PG14, PE14, PE6 ---
  GPIO_InitStruct.Pin = ENC1_A_Pin;
  HAL_GPIO_Init(ENC1_A_GPIO_Port, &GPIO_InitStruct); // PG14 CN10 D2
  GPIO_InitStruct.Pin = ENC1_B_Pin;
  HAL_GPIO_Init(ENC1_B_GPIO_Port, &GPIO_InitStruct); // PE14 CN10 D4
  GPIO_InitStruct.Pin = ENC1_Z_Pin;
  HAL_GPIO_Init(ENC1_Z_GPIO_Port, &GPIO_InitStruct); // PE6  CN10 D38

  // --- CN7 I/O: PG9 + CN9 D64 PD10 ---
  GPIO_InitStruct.Pin = ENC2_A_Pin;
  HAL_GPIO_Init(ENC2_A_GPIO_Port, &GPIO_InitStruct); // PG9  CN7 D8
  GPIO_InitStruct.Pin = ENC2_B_Pin;
  HAL_GPIO_Init(ENC2_B_GPIO_Port, &GPIO_InitStruct); // PD10 CN9 D64
  GPIO_InitStruct.Pin = ENC2_Z_Pin;
  HAL_GPIO_Init(ENC2_Z_GPIO_Port, &GPIO_InitStruct); // PG6  CN10 D26

  // --- CN10 QSPI: PD13, PD12, PD11 ---
  GPIO_InitStruct.Pin = ENC3_A_Pin;
  HAL_GPIO_Init(ENC3_A_GPIO_Port, &GPIO_InitStruct); // PD13 CN10 D28
  GPIO_InitStruct.Pin = ENC3_B_Pin;
  HAL_GPIO_Init(ENC3_B_GPIO_Port, &GPIO_InitStruct); // PD12 CN10 D29
  GPIO_InitStruct.Pin = ENC3_Z_Pin;
  HAL_GPIO_Init(ENC3_Z_GPIO_Port, &GPIO_InitStruct); // PD11 CN10 D30

  // --- ADC: PF6, PF10, PC0, PF11 ---
  GPIO_InitStruct.Pin = ENC4_A_Pin;
  HAL_GPIO_Init(ENC4_A_GPIO_Port, &GPIO_InitStruct); // PF6  CN10 A6
  GPIO_InitStruct.Pin = ENC4_B_Pin;
  HAL_GPIO_Init(ENC4_B_GPIO_Port, &GPIO_InitStruct); // PF10 CN10 A7
  GPIO_InitStruct.Pin = ENC4_Z_Pin;
  HAL_GPIO_Init(ENC4_Z_GPIO_Port, &GPIO_InitStruct); // PC0  CN9 A1
  GPIO_InitStruct.Pin = ENC5_A_Pin;
  HAL_GPIO_Init(ENC5_A_GPIO_Port, &GPIO_InitStruct); // PF11 CN9 A5

  // --- I2C2 + SAI on CN9: PF14, PF15, PF8, PF7, PF9 ---
  GPIO_InitStruct.Pin = ENC5_B_Pin;
  HAL_GPIO_Init(ENC5_B_GPIO_Port, &GPIO_InitStruct); // PF14 CN9 D69
  GPIO_InitStruct.Pin = ENC5_Z_Pin;
  HAL_GPIO_Init(ENC5_Z_GPIO_Port, &GPIO_InitStruct); // PF15 CN9 D68
  GPIO_InitStruct.Pin = ENC6_A_Pin;
  HAL_GPIO_Init(ENC6_A_GPIO_Port, &GPIO_InitStruct); // PF8  CN9 D61
  GPIO_InitStruct.Pin = ENC6_B_Pin;
  HAL_GPIO_Init(ENC6_B_GPIO_Port, &GPIO_InitStruct); // PF7  CN9 D62
  GPIO_InitStruct.Pin = ENC6_Z_Pin;
  HAL_GPIO_Init(ENC6_Z_GPIO_Port, &GPIO_InitStruct); // PF9  CN9 D63
}

uint32_t Encoder_Test_SampleAll(void)
{
  uint32_t v = 0;
  v |= (HAL_GPIO_ReadPin(ENC1_A_GPIO_Port, ENC1_A_Pin) == GPIO_PIN_SET ? 1U <<  0 : 0);
  v |= (HAL_GPIO_ReadPin(ENC1_B_GPIO_Port, ENC1_B_Pin) == GPIO_PIN_SET ? 1U <<  1 : 0);
  v |= (HAL_GPIO_ReadPin(ENC1_Z_GPIO_Port, ENC1_Z_Pin) == GPIO_PIN_SET ? 1U <<  2 : 0);
  v |= (HAL_GPIO_ReadPin(ENC2_A_GPIO_Port, ENC2_A_Pin) == GPIO_PIN_SET ? 1U <<  3 : 0);
  v |= (HAL_GPIO_ReadPin(ENC2_B_GPIO_Port, ENC2_B_Pin) == GPIO_PIN_SET ? 1U <<  4 : 0);
  v |= (HAL_GPIO_ReadPin(ENC2_Z_GPIO_Port, ENC2_Z_Pin) == GPIO_PIN_SET ? 1U <<  5 : 0);
  v |= (HAL_GPIO_ReadPin(ENC3_A_GPIO_Port, ENC3_A_Pin) == GPIO_PIN_SET ? 1U <<  6 : 0);
  v |= (HAL_GPIO_ReadPin(ENC3_B_GPIO_Port, ENC3_B_Pin) == GPIO_PIN_SET ? 1U <<  7 : 0);
  v |= (HAL_GPIO_ReadPin(ENC3_Z_GPIO_Port, ENC3_Z_Pin) == GPIO_PIN_SET ? 1U <<  8 : 0);
  v |= (HAL_GPIO_ReadPin(ENC4_A_GPIO_Port, ENC4_A_Pin) == GPIO_PIN_SET ? 1U <<  9 : 0);
  v |= (HAL_GPIO_ReadPin(ENC4_B_GPIO_Port, ENC4_B_Pin) == GPIO_PIN_SET ? 1U << 10 : 0);
  v |= (HAL_GPIO_ReadPin(ENC4_Z_GPIO_Port, ENC4_Z_Pin) == GPIO_PIN_SET ? 1U << 11 : 0);
  v |= (HAL_GPIO_ReadPin(ENC5_A_GPIO_Port, ENC5_A_Pin) == GPIO_PIN_SET ? 1U << 12 : 0);
  v |= (HAL_GPIO_ReadPin(ENC5_B_GPIO_Port, ENC5_B_Pin) == GPIO_PIN_SET ? 1U << 13 : 0);
  v |= (HAL_GPIO_ReadPin(ENC5_Z_GPIO_Port, ENC5_Z_Pin) == GPIO_PIN_SET ? 1U << 14 : 0);
  v |= (HAL_GPIO_ReadPin(ENC6_A_GPIO_Port, ENC6_A_Pin) == GPIO_PIN_SET ? 1U << 15 : 0);
  v |= (HAL_GPIO_ReadPin(ENC6_B_GPIO_Port, ENC6_B_Pin) == GPIO_PIN_SET ? 1U << 16 : 0);
  v |= (HAL_GPIO_ReadPin(ENC6_Z_GPIO_Port, ENC6_Z_Pin) == GPIO_PIN_SET ? 1U << 17 : 0);
  return v;
}
