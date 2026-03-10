/* Test T6c: SLOWER blinks - tcpip_init BEFORE osKernelStart */
#include "main.h"
#include "cmsis_os.h"
#include "lwip/tcpip.h"

volatile int callback_done = 0;

void tcpip_done_cb(void *arg) {
    callback_done = 1;
    /* Signal callback - blink RED LED 5 times */
    for(int j = 0; j < 5; j++) {
        GPIOB->BSRR = (1 << 14);  // RED ON
        for(volatile int i = 0; i < 1500000; i++);
        GPIOB->BSRR = (1 << 30);  // RED OFF
        for(volatile int i = 0; i < 1500000; i++);
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
  .stack_size = 2048,
  .priority = osPriorityNormal,
};

void StartDefaultTask(void *argument)
{
  /* 5 blinks = task started */
  for(int j = 0; j < 5; j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int i = 0; i < 1500000; i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int i = 0; i < 1500000; i++);
  }
  
  /* Continuous blink = main loop */
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
  /* Enable GPIOB clock */
  RCC->AHB4ENR |= (1 << 1);
  for(volatile int i = 0; i < 1000; i++);

  /* Set PB0 (GREEN) as output */
  GPIOB->MODER &= ~(3 << 0);
  GPIOB->MODER |= (1 << 0);
  
  /* Set PB14 (RED) as output */
  GPIOB->MODER &= ~(3 << 28);
  GPIOB->MODER |= (1 << 28);

  /* 1 blink = start (SLOWER) */
  GPIOB->BSRR = (1 << 0);
  for(volatile int i = 0; i < 1500000; i++);
  GPIOB->BSRR = (1 << 16);
  for(volatile int i = 0; i < 1500000; i++);
  for(volatile int i = 0; i < 1500000; i++);  // extra pause

  HAL_Init();
  SystemClock_Config();
  SCB->VTOR = 0x08000000;

  /* 2 blinks = init done (SLOWER) */
  for(int j = 0; j < 2; j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int i = 0; i < 1500000; i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int i = 0; i < 1500000; i++);
  }
  for(volatile int i = 0; i < 1500000; i++);  // extra pause

  osKernelInitialize();
  MX_FREERTOS_Init();
  
  /* 3 blinks = BEFORE osKernelStart (SLOWER) */
  for(int j = 0; j < 3; j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int i = 0; i < 1500000; i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int i = 0; i < 1500000; i++);
  }
  for(volatile int i = 0; i < 1500000; i++);  // extra pause
  
  /* Call tcpip_init BEFORE osKernelStart */
  tcpip_init(tcpip_done_cb, NULL);
  
  /* 4 blinks = after tcpip_init (SLOWER) */
  for(int j = 0; j < 4; j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int i = 0; i < 1500000; i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int i = 0; i < 1500000; i++);
  }
  for(volatile int i = 0; i < 1500000; i++);  // extra pause
  
  osKernelStart();

  while(1);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) { }
void Error_Handler(void) { while(1); }
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { while(1); }
#endif
