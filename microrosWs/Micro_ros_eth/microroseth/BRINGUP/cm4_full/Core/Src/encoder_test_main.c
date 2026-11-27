/*
 * Encoder test main — builds on top of existing CM4 HAL, adds 18 general FT GPIOs.
 * Does NOT modify existing main.c, gpio.c, MicroRosEth.ioc — new test binary.
 * Verifies: 18 pins init, sample, blink GREEN if all high (pull-up), RED otherwise.
 * Timers PWM untouched: no TIM AF used.
 */

#include "main.h"
#include "gpio.h"
#include "encoder_pins.h"
#include "tim.h"
#include "spi.h"
#include "shared_data.h"
#include <string.h>

static void SystemClock_Config(void);

__attribute__((section(".shared"))) shared_data_t shared_data_inst;
__attribute__((section(".shared"))) char encoder_test_shared[64];

int main(void)
{
  extern uint32_t g_pfnVectors;
  SCB->VTOR = (uint32_t)&g_pfnVectors;

  HAL_Init();
  SystemClock_Config();

  // DWT for DelayMs via TIM2 still works
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  MX_GPIO_Init();          // existing LEDs, brakes, DRV_CS/RST, button
  MX_Encoder_GPIO_Init();  // NEW 18 general FT pins
  MX_TIM2_Init();          // 1 MHz tick
  MX_SPI1_Init();          // existing powerSTEP01

  // Quick alive blink — confirms existing GPIOs still ok
  for (int i=0;i<3;i++){ LED_GREEN_ON(); LED_RED_ON(); for(volatile int d=0;d<800000;d++); LED_GREEN_OFF(); LED_RED_OFF(); for(volatile int d=0;d<800000;d++); }

  // Verify 18 encoders pull-up: sample should be all 1s if encoders disconnected (FT pull-up)
  uint32_t s = Encoder_Test_SampleAll();
  if (s == 0x3FFFFU) { // 18 bits = 262143
    // All high — 3 green blinks
    for(int i=0;i<3;i++){ LED_GREEN_ON(); for(volatile int d=0;d<500000;d++); LED_GREEN_OFF(); for(volatile int d=0;d<500000;d++); }
  } else {
    // Some low — 2 red blinks showing sample != all-high (e.g. encoder pulling low)
    for(int i=0;i<2;i++){ LED_RED_ON(); for(volatile int d=0;d<500000;d++); LED_RED_OFF(); for(volatile int d=0;d<500000;d++); }
  }

  // Loop: sample 18 at 100 Hz, update LEDs, keep powerSTEP01 idle (no PWM/TIM change)
  uint32_t last = DWT->CYCCNT;
  uint32_t cyc_100ms = SystemCoreClock / 10U;
  while (1) {
    uint32_t now = DWT->CYCCNT;
    if ((now - last) >= cyc_100ms) {
      last = now;
      s = Encoder_Test_SampleAll();
      // Encode LSB on LEDs: GREEN = bit0 high, RED = bit0 low (sanity)
      if (s & 1U) { LED_GREEN_ON(); LED_RED_OFF(); } else { LED_RED_ON(); LED_GREEN_OFF(); }
      // Also publish to shared for CM7 inspection
      encoder_test_shared[0] = (char)(s & 0xFF);
    }
    // Keep existing brake/CS idle — prove no regression
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);
  }
}

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY));
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
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
}

void Error_Handler(void)
{
  __disable_irq();
  while(1){ LED_RED_ON(); for(volatile int i=0;i<900000;i++); LED_RED_OFF(); for(volatile int i=0;i<900000;i++); }
}

// SysTick handled in stm32h7xx_it.c
