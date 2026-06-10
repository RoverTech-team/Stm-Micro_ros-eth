#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdio.h>

void Error_Handler(void);

#define LED_GREEN_Pin        GPIO_PIN_0
#define LED_GREEN_GPIO_Port  GPIOB

#define LED_RED_Pin          GPIO_PIN_14
#define LED_RED_GPIO_Port    GPIOB

#define DRV_RESET_Pin        GPIO_PIN_8
#define DRV_RESET_GPIO_Port  GPIOA

#define DRV_CS_Pin           GPIO_PIN_9
#define DRV_CS_GPIO_Port     GPIOB

#define J2_BRAKE_Pin         GPIO_PIN_4
#define J2_BRAKE_GPIO_Port   GPIOA

#define J3_BRAKE_Pin         GPIO_PIN_1
#define J3_BRAKE_GPIO_Port   GPIOB

#define LED_GREEN_ON()   HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET)
#define LED_GREEN_OFF()  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET)
#define LED_RED_ON()     HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   GPIO_PIN_SET)
#define LED_RED_OFF()    HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   GPIO_PIN_RESET)

#ifdef __cplusplus
}
#endif

#endif
