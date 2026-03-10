/**
  ******************************************************************************
  * @file           : main_smoke_dualcore_cm7.c
  * @brief          : CM7-first Renode smoke firmware for STM32H755ZI-Q
  ******************************************************************************
  */

#include "stm32h7xx_hal.h"

#define UART_BAUDRATE 115200U
#define MAILBOX_ADDR  ((volatile uint32_t *)0x10047F00U)

#define MAILBOX_EMPTY         0x00000000U
#define MAILBOX_CM7_RELEASED  0xC7AA0001U
#define MAILBOX_CM4_ACKED     0xC4AA0002U

static void BusyDelay(volatile uint32_t count);
static void ConsoleWrite(const char *text);
static void ConsoleWriteLine(const char *text);
static void GPIO_Init(void);
static void UART_Init(void);
static void HSEM_Init(void);
static void HSEM_FastTakeRelease(uint32_t semId);

int main(void)
{
    uint32_t ticks = 0;

    SCB->VTOR = 0x08000000U;

    GPIO_Init();
    UART_Init();
    HSEM_Init();

    *MAILBOX_ADDR = MAILBOX_EMPTY;

    ConsoleWriteLine("CM7: boot");
    ConsoleWriteLine("CM7: uart2-ready");
    ConsoleWriteLine("CM7: releasing-cm4");

    HSEM_FastTakeRelease(0U);
    *MAILBOX_ADDR = MAILBOX_CM7_RELEASED;

    while(*MAILBOX_ADDR != MAILBOX_CM4_ACKED)
    {
        BusyDelay(250000U);
    }

    ConsoleWriteLine("CM7: cm4-acked");

    while(1)
    {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
        ConsoleWriteLine((ticks++ & 1U) == 0U ? "CM7: tick-even" : "CM7: tick-odd");
        BusyDelay(1200000U);
    }
}

static void BusyDelay(volatile uint32_t count)
{
    while(count-- > 0U)
    {
        __asm volatile ("nop");
    }
}

static void ConsoleWrite(const char *text)
{
    uint32_t i = 0U;

    while(text[i] != '\0')
    {
        while((USART2->ISR & USART_ISR_TXE_TXFNF) == 0U)
        {
        }
        USART2->TDR = (uint8_t)text[i++];
    }
}

static void ConsoleWriteLine(const char *text)
{
    ConsoleWrite(text);
    ConsoleWrite("\r\n");
}

static void GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);

    gpio.Pin = GPIO_PIN_14;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static void UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    USART2->CR1 = 0U;
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;
    USART2->PRESC = 0U;
    USART2->BRR = 125000000U / UART_BAUDRATE;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void HSEM_Init(void)
{
    __HAL_RCC_HSEM_CLK_ENABLE();
}

static void HSEM_FastTakeRelease(uint32_t semId)
{
    (void)HSEM->RLR[semId];
    HSEM->R[semId] = HSEM_CR_COREID_CURRENT;
}

void Error_Handler(void)
{
    __disable_irq();
    while(1)
    {
    }
}
