#include "main.h"
#include "stm32h7xx_it.h"
#include "shared_data.h"

static void capture_fault_frame(uint32_t *stack, uint8_t kind);

void NMI_Handler(void)
{
  while (1) {
    ;
  }
}

__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile (
    "tst lr, #4        \n"
    "ite eq             \n"
    "mrseq r0, msp      \n"
    "mrsne r0, psp      \n"
    "b hard_fault_c     \n"
  );
}

__attribute__((naked)) void MemManage_Handler(void)
{
  __asm volatile (
    "tst lr, #4        \n"
    "ite eq             \n"
    "mrseq r0, msp      \n"
    "mrsne r0, psp      \n"
    "b mem_manage_c     \n"
  );
}

__attribute__((naked)) void BusFault_Handler(void)
{
  __asm volatile (
    "tst lr, #4        \n"
    "ite eq             \n"
    "mrseq r0, msp      \n"
    "mrsne r0, psp      \n"
    "b bus_fault_c      \n"
  );
}

__attribute__((naked)) void UsageFault_Handler(void)
{
  __asm volatile (
    "tst lr, #4        \n"
    "ite eq             \n"
    "mrseq r0, msp      \n"
    "mrsne r0, psp      \n"
    "b usage_fault_c    \n"
  );
}

__attribute__((used)) static void hard_fault_c(uint32_t *stack)  { capture_fault_frame(stack, 1); }
__attribute__((used)) static void mem_manage_c(uint32_t *stack)  { capture_fault_frame(stack, 2); }
__attribute__((used)) static void bus_fault_c(uint32_t *stack)   { capture_fault_frame(stack, 3); }
__attribute__((used)) static void usage_fault_c(uint32_t *stack) { capture_fault_frame(stack, 4); }

static void capture_fault_frame(uint32_t *stack, uint8_t kind)
{
  SHARED_DATA->last_fault_cfsr = SCB->CFSR;
  SHARED_DATA->last_fault_hfsr = SCB->HFSR;
  SHARED_DATA->last_fault_mmar = SCB->MMFAR;
  SHARED_DATA->last_fault_bfar = SCB->BFAR;
  SHARED_DATA->last_fault_lr   = stack[5];
  SHARED_DATA->last_fault_pc   = stack[6];
  SHARED_DATA->last_fault_ipsr = __get_IPSR();
  SHARED_DATA->last_fault_cfb  = (SHARED_DATA->last_fault_cfb & 0xFFFFFF00U) | kind;
  __DSB();

  LED_RED_ON();
  for (volatile uint32_t i = 0; i < 1500000U; i++) {
    ;
  }
  LED_RED_OFF();
  for (volatile uint32_t i = 0; i < 1500000U; i++) {
    ;
  }
  NVIC_SystemReset();
}

void SVC_Handler(void)       { }
void DebugMon_Handler(void)  { }
void PendSV_Handler(void)    { }

void SysTick_Handler(void)
{
  HAL_IncTick();
}
