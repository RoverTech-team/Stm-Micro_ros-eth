/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pump.h
  * @brief   Pump PWM control (MOSFET gate) for the CM7 micro-ROS firmware.
  *          Ported from the Arduino/RP2040 reference.c logic.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __PUMP_H__
#define __PUMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

/* USER CODE BEGIN Configurable defines */
/* Hardware assignment for the PWM that drives the MOSFET gate.
 * Default per stm32h755ziq_pin_mapping.md: CN7 pin 18 (D9) = PD15 = TIM4_CH4.
 * Chosen because TIM1 is reserved as the HAL timebase and PB0 (TIM3_CH3) is the
 * green status LED. Change these to match your board wiring.
 * Other documented, conflict-free PWM pins:
 *   PA0  = TIM2_CH1 (CN10 D32), PB5  = TIM3_CH2 (CN7 D11),
 *   PB10 = TIM2_CH3 (CN10 D36), PB11 = TIM2_CH4 (CN10 D35),
 *   PD14 = TIM4_CH3 (CN7 D10). */
#define PUMP_TIMER_INSTANCE    TIM4
#define PUMP_PWM_CHANNEL       TIM_CHANNEL_4
#define PUMP_GPIO_PORT         GPIOD
#define PUMP_GPIO_PIN          GPIO_PIN_15
#define PUMP_GPIO_AF           GPIO_AF2_TIM4

/* PWM frequency in Hz and resolution (period). 8-bit resolution (0..255)
 * mimics the original Arduino analogWrite() range. Frequency set to 1000 Hz
 * to match the RP2040/Arduino analogWrite() default (the reference ran 50 %
 * slower at the original 10 kHz due to the bare-GPIO MOSFET gate drive). */
#define PUMP_PWM_FREQUENCY_HZ  1000U
#define PUMP_PWM_PERIOD        255U
/* USER CODE END Configurable defines */

/* micro-ROS topic the pump setpoint (0-100 %) is received on. */
#define PUMP_TOPIC_NAME        "pump_setpoint"

void Pump_Init(void);
void Pump_SetPercent(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* __PUMP_H__ */
