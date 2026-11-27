/*
 * CM4 GPIO init — restored from 0d8dec9 mapping.
 *
 * All pin definitions live in main.h; this file only calls HAL_GPIO_Init.
 */

#include "gpio.h"

void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  GPIO_InitStruct.Pin   = LED_GREEN_Pin | LED_RED_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GREEN_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin | LED_RED_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = DRV_CS_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DRV_CS_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin   = DRV_RESET_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DRV_RESET_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(DRV_RESET_GPIO_Port, DRV_RESET_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = J2_BRAKE_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(J2_BRAKE_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(J2_BRAKE_GPIO_Port, J2_BRAKE_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = J3_BRAKE_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(J3_BRAKE_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(J3_BRAKE_GPIO_Port, J3_BRAKE_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = BUTTON_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull  = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUTTON_GPIO_Port, &GPIO_InitStruct);
}
