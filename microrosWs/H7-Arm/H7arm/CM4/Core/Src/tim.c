#include "tim.h"

#define TIMER_PRESCALER_1MHZ 223U

uint32_t Timer2_NowUs(void)
{
  return TIM2->CNT;
}

void DelayUs(uint32_t delay_us)
{
  uint32_t start = Timer2_NowUs();
  while ((uint32_t)(Timer2_NowUs() - start) < delay_us) {
    ;
  }
}

void DelayMs(uint32_t delay_ms)
{
  DelayUs(delay_ms * 1000U);
}

void MX_TIM2_Init(void)
{
  RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;
  TIM2->CR1 = 0U;
  TIM2->PSC = TIMER_PRESCALER_1MHZ;
  TIM2->ARR = 0xFFFFFFFFU;
  TIM2->CNT = 0U;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CR1 = TIM_CR1_CEN;
}
