/* Pump PWM self-test firmware (variant of main.c).
 * At boot it drives the MOSFET-gate PWM through a fixed sequence:
 *   50 % for 30 s, then 0 % for 30 s, then 100 % for 10 s, then repeats.
 * No micro-ROS / network required; useful to verify the PWM hardware wiring.
 */

#include "main.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "pump.h"

static void SetGreenLed(bool enabled)
{
  if(enabled)
  {
    GPIOB->BSRR = (1u << 0);
  }
  else
  {
    GPIOB->BSRR = (1u << 16);
  }
}

static void SetRedLed(bool enabled)
{
  if(enabled)
  {
    GPIOB->BSRR = (1u << 14);
  }
  else
  {
    GPIOB->BSRR = (1u << 30);
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

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

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                               | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                               | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if(htim != NULL && htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
}

void Error_Handler(void)
{
  SetGreenLed(false);
  while(1)
  {
    SetRedLed(true);
    HAL_Delay(250);
    SetRedLed(false);
    HAL_Delay(250);
  }
}

int main(void)
{
  RCC->AHB4ENR |= (1u << 1);
  for(volatile int i = 0; i < 10000; i++) {}

  GPIOB->MODER &= ~(3u << 0);
  GPIOB->MODER |= (1u << 0);
  GPIOB->MODER &= ~(3u << 28);
  GPIOB->MODER |= (1u << 28);

  SetGreenLed(true);
  SetRedLed(false);

  HAL_Init();
  SystemClock_Config();
  SCB->VTOR = 0x08000000;

  Debug_USART3_Init();
  Debug_USART3_Print("CM7: pump-test-boot\r\n");
  printf("CM7: pump-test hal-ok\r\n");

  Pump_Init();
  printf("CM7: pump-init-ok pin=%d\r\n", (int)PUMP_GPIO_PIN);

  for(;;)
  {
    /* Ramp 45 % -> 100 % in 10 % steps, 5 s each, green LED blinking. */
    for(int pct = 45; pct < 100; pct += 10)
    {
      Pump_SetPercent((uint8_t)pct);
      SetRedLed(false);
      printf("CM7: pump=%d%%\r\n", pct);
      for(int i = 0; i < 10; i++)
      {
        SetGreenLed((i % 2) == 0);
        HAL_Delay(500);
      }
    }

    /* Final 100 % step */
    Pump_SetPercent(100);
    SetRedLed(false);
    printf("CM7: pump=100%%\r\n");
    for(int i = 0; i < 10; i++)
    {
      SetGreenLed((i % 2) == 0);
      HAL_Delay(500);
    }
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
  Error_Handler();
}
#endif
