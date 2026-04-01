/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Minimal CM4 ultrasonic producer for shared SRAM4
 ******************************************************************************
 */

#include "main.h"
#include "shared_data.h"
#include <string.h>

#define TIMER_PRESCALER_1MHZ 223U
#define TRIG_RESET_TIME_US 2U
#define TRIG_PULSE_TIME_US 10U
#define ECHO_WAIT_TIMEOUT_US 30000U
#define ECHO_PULSE_TIMEOUT_US 60000U
#define SENSOR_MAX_DISTANCE_CM 600U
#define SENSOR_STARTUP_SETTLE_MS 300U

#define BUSY(n)                                                                \
  do {                                                                         \
    for (volatile int _i = 0; _i < (n); _i++)                                  \
      ;                                                                        \
  } while (0)

#define LED_GREEN_ON() (GPIOB->BSRR = GPIO_PIN_0)
#define LED_GREEN_OFF() (GPIOB->BSRR = (GPIO_PIN_0 << 16))
#define LED_RED_ON() (GPIOB->BSRR = GPIO_PIN_14)
#define LED_RED_OFF() (GPIOB->BSRR = (GPIO_PIN_14 << 16))
#define TRIG_HIGH() (GPIOD->BSRR = GPIO_PIN_1)
#define TRIG_LOW() (GPIOD->BSRR = (GPIO_PIN_1 << 16))
#define ECHO_IS_HIGH() (((GPIOD->IDR & GPIO_PIN_0) != 0U) ? 1U : 0U)

static uint32_t Timer2_NowUs(void) { return TIM2->CNT; }

static void DelayMs(uint32_t delay_ms) {
  uint32_t start_us = Timer2_NowUs();
  uint32_t delay_us = delay_ms * 1000U;

  while ((uint32_t)(Timer2_NowUs() - start_us) < delay_us)
    ;
}

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

int main(void) {
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIODEN;
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

  SCB->VTOR = 0x08100000;

  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIODEN;
  RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;

  GPIOD->MODER &= ~((3U << (0 * 2U)) | (3U << (1 * 2U)));
  GPIOD->MODER |= (1U << (1 * 2U));
  GPIOD->PUPDR &= ~((3U << (0 * 2U)) | (3U << (1 * 2U)));
  GPIOD->PUPDR |= (2U << (0 * 2U));
  GPIOD->OTYPER &= ~(1U << 1);
  GPIOD->OSPEEDR &= ~((3U << (0 * 2U)) | (3U << (1 * 2U)));

  TIM2->CR1 = 0U;
  TIM2->PSC = TIMER_PRESCALER_1MHZ;
  TIM2->ARR = 0xFFFFFFFFU;
  TIM2->CNT = 0U;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CR1 = TIM_CR1_CEN;

  memset((void *)SHARED_DATA, 0, sizeof(*SHARED_DATA));
  SHARED_DATA->cm4_write_seq = 1U;
  __DSB();
  TRIG_LOW();
  DelayMs(SENSOR_STARTUP_SETTLE_MS);

  uint32_t dist_cm = 100U;

  while (1) {
    if (Sensor_Measure(&dist_cm)) {
      LED_GREEN_ON();
      LED_RED_OFF();
    } else {
      LED_GREEN_OFF();
      LED_RED_ON();
    }

    DelayMs(500U);
  }
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
