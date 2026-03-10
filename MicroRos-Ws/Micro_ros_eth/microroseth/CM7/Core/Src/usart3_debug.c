#include "main.h"

void Debug_USART3_Init(void)
{
  COM_InitTypeDef com = {0};

  com.BaudRate = 115200;
  com.WordLength = COM_WORDLENGTH_8B;
  com.StopBits = COM_STOPBITS_1;
  com.Parity = COM_PARITY_NONE;
  com.HwFlowCtl = COM_HWCONTROL_NONE;

  if(BSP_COM_Init(COM1, &com) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  (void)BSP_COM_SelectLogPort(COM1);
}

void Debug_USART3_Print(const char *message)
{
  if(message != NULL)
  {
    printf("%s", message);
  }
}
