#include "main.h"

#ifndef CM7_SERIAL_SILENT
#define CM7_SERIAL_SILENT 1
#endif

void Debug_USART3_Init(void)
{
#if CM7_SERIAL_SILENT
  return;
#else
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
#endif
}

void Debug_USART3_Print(const char *message)
{
#if CM7_SERIAL_SILENT
  (void)message;
#else
  if(message != NULL)
  {
    printf("%s", message);
  }
#endif
}
