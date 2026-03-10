/* Test T23: Manual LwIP init + simple Ethernet init */
#include "main.h"
#include "cmsis_os.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/stats.h"

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

osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 4096,
  .priority = osPriorityNormal,
};

void StartDefaultTask(void *argument)
{
  /* 3 blinks = task started */
  for(int j = 0; j < 3; j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int i = 0; i < 2000000; i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int i = 0; i < 2000000; i++);
  }
  for(volatile int i = 0; i < 3000000; i++);
  
  /* Manual LwIP init */
  stats_init();
  sys_init();
  mem_init();
  memp_init();
  pbuf_init();
  netif_init();
  sys_timeouts_init();
  
  /* 4 blinks = LwIP done */
  for(int j = 0; j < 4; j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int i = 0; i < 2000000; i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int i = 0; i < 2000000; i++);
  }
  for(volatile int i = 0; i < 3000000; i++);
  
  /* Init Ethernet - just call the HAL init */
  MX_ETH_Init();
  
  /* 5 blinks = ETH init done */
  for(int j = 0; j < 5; j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int i = 0; i < 2000000; i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int i = 0; i < 2000000; i++);
  }
  for(volatile int i = 0; i < 3000000; i++);
  
  /* 5 RED blinks = SUCCESS */
  for(int j = 0; j < 5; j++) {
    GPIOB->BSRR = (1 << 14);
    for(volatile int i = 0; i < 2000000; i++);
    GPIOB->BSRR = (1 << 30);
    for(volatile int i = 0; i < 2000000; i++);
  }
  
  /* Main loop */
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

int main(void)
{
  RCC->AHB4ENR |= (1 << 1);
  for(volatile int i = 0; i < 10000; i++);

  GPIOB->MODER &= ~(3 << 0);
  GPIOB->MODER |= (1 << 0);
  GPIOB->MODER &= ~(3 << 28);
  GPIOB->MODER |= (1 << 28);

  /* LED VALIDATION */
  for(int j = 0; j < 2; j++) {
    GPIOB->BSRR = (1 << 0);
    GPIOB->BSRR = (1 << 14);
    for(volatile int i = 0; i < 3000000; i++);
    GPIOB->BSRR = (1 << 16);
    GPIOB->BSRR = (1 << 30);
    for(volatile int i = 0; i < 3000000; i++);
  }
  for(volatile int i = 0; i < 5000000; i++);

  GPIOB->BSRR = (1 << 0);
  for(volatile int i = 0; i < 2000000; i++);
  GPIOB->BSRR = (1 << 16);
  for(volatile int i = 0; i < 3000000; i++);

  HAL_Init();
  SystemClock_Config();
  SCB->VTOR = 0x08000000;

  for(int j = 0; j < 2; j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int i = 0; i < 2000000; i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int i = 0; i < 2000000; i++);
  }
  for(volatile int i = 0; i < 3000000; i++);

  osKernelInitialize();
  MX_FREERTOS_Init();
  osKernelStart();

  while(1);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) { }
void Error_Handler(void) { while(1); }
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { while(1); }
#endif
