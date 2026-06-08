#ifndef TIM_H
#define TIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void     MX_TIM2_Init(void);
uint32_t Timer2_NowUs(void);
void     DelayUs(uint32_t delay_us);
void     DelayMs(uint32_t delay_ms);

#ifdef __cplusplus
}
#endif

#endif
