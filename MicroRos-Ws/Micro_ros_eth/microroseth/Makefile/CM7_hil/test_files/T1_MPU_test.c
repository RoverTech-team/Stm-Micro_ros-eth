/**
 * T1: MPU Configuration Test
 * 
 * Purpose: Verify MPU_Config() alone doesn't break FreeRTOS
 * 
 * Expected Output:
 *   1 blink  = before MPU (starting)
 *   2 blinks = after MPU config
 *   3 blinks = after HAL_Init + SystemClock
 *   Continuous slow blink = FreeRTOS task running (SUCCESS)
 * 
 * If stuck: Note how many blinks you see
 */

#include "main.h"
#include "cmsis_os.h"

void SystemClock_Config(void);
void MPU_Config(void);

/*-----------------------------------------------------------*/
/* System Clock Configuration - 224 MHz from PLL             */
/*-----------------------------------------------------------*/
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

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
}

/*-----------------------------------------------------------*/
/* MPU Configuration - Ethernet DMA region                   */
/*-----------------------------------------------------------*/
void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  HAL_MPU_Disable();

  /* Configure MPU for Ethernet DMA buffers at 0x30000000 */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x30000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/*-----------------------------------------------------------*/
/* FreeRTOS Task                                             */
/*-----------------------------------------------------------*/
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 2048,
  .priority = osPriorityNormal,
};

void StartDefaultTask(void *argument)
{
  /* 5 blinks = task running */
  for(int j = 0; j < 5; j++) {
    GPIOB->BSRR = (1 << 0);
    osDelay(200);
    GPIOB->BSRR = (1 << 16);
    osDelay(200);
  }

  /* Continuous slow blink = SUCCESS */
  for(;;)
  {
    GPIOB->BSRR = (1 << 0);
    osDelay(500);
    GPIOB->BSRR = (1 << 16);
    osDelay(500);
  }
}

void MX_FREERTOS_Init(void) {
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
}

/*-----------------------------------------------------------*/
/* Main Entry                                                */
/*-----------------------------------------------------------*/
int main(void)
{
  /* Enable GPIOB clock early */
  RCC->AHB4ENR |= (1 << 1);
  for(volatile int i = 0; i < 1000; i++);

  /* Configure PB0 as output */
  GPIOB->MODER &= ~(3 << 0);
  GPIOB->MODER |= (1 << 0);

  /* 1 blink = starting, before MPU */
  GPIOB->BSRR = (1 << 0);
  for(volatile int i = 0; i < 400000; i++);
  GPIOB->BSRR = (1 << 16);
  for(volatile int i = 0; i < 400000; i++);

  /* Configure MPU */
  MPU_Config();

  /* 2 blinks = MPU configured */
  for(int j = 0; j < 2; j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int i = 0; i < 400000; i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int i = 0; i < 400000; i++);
  }

  /* Initialize HAL and clocks */
  HAL_Init();
  SystemClock_Config();
  SCB->VTOR = 0x08000000;

  /* 3 blinks = HAL + Clock configured */
  for(int j = 0; j < 3; j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int i = 0; i < 400000; i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int i = 0; i < 400000; i++);
  }

  /* Start FreeRTOS */
  osKernelInitialize();
  MX_FREERTOS_Init();
  osKernelStart();

  /* Should not reach here */
  while(1);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) { }
void Error_Handler(void) { while(1); }
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { while(1); }
#endif
