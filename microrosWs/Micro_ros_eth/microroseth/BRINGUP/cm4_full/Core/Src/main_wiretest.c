/**
 * CM4 WireTest — one joint at a time: +10 deg, hold, back to start, next joint.
 * Sequence: J1 -> J2 -> J3 -> J4 -> J5 -> J6 -> repeat.
 * Only the active joint is commanded; all others stay energised and holding
 * (no HiZ), J2/J3 brakes are released so they can move on their turn.
 * Values from setups.h (Archivio reference, updated 2026-11-04).
 */
#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "tim.h"
#include "drivers/RArm/rarm.h"
#include "drivers/powerSTEP01/ps01.h"
#include "shared_data.h"
#include "setups.h"
#include <string.h>

static void SystemClock_Config(void);

#define MOTOR_LOOP_PERIOD_MS 10U
#define STEP_DEG             10      /* per-joint move amplitude, degrees        */
#define START_DELAY_MS       3000U   /* settle time before the first move       */
#define MOVE_TIMEOUT_MS      30000U  /* wait for BUSY clear; J1 10deg ~12 s at 1000 st/s */
#define SETTLE_MS            1500U   /* dwell at each endpoint                  */
#define WT_DIAG              0       /* 1 = read back driver regs and idle (NO motion) */

__attribute__((section(".shared"))) shared_data_t shared_data_inst;

extern const PS01_OS_t *ps01_baremetal_get_os(void);

static Stepper_t motors[N_JOINTS];
static StepperBank_t bank = { .motors = motors, .active = 0 };

static uint8_t rr_faults[N_JOINTS];
static uint8_t rr_joint = 0xFF, rr_blinks=0, rr_total=0, rr_phase=0, rr_pause=0;
static uint32_t rr_timer=0;
static uint8_t btn_prev=1; static uint32_t btn_last_ms=0;
static uint8_t btn_green_blinks=0, btn_green_phase=0; static uint32_t btn_green_timer=0;
/* One-joint-at-a-time sequencer:
 * phase 0 = command +STEP, 1 = wait for BUSY clear, 2 = dwell,
 * phase 3 = command -STEP (back to start), 4 = wait, 5 = dwell, then next joint. */
static uint8_t  test_joint = 0;
static uint8_t  test_phase = 0;
static uint32_t test_timer = 0;

static void RR_Tick(uint32_t now);
static void RR_PollFaults(void);
static uint8_t RR_BlinkCount(uint16_t status);
static uint8_t RR_AlarmByte(uint16_t status);
static uint8_t RR_NextFaulted(uint8_t start);
static void Motor_UpdatePositions(void);

/* Bring-up trace: stage number parked in the legacy distance_cm field
 * (unused by this build) so a hang in setup can be located over SWD. */
#define WT_STAGE(n) do { shared_data_inst.distance_cm = (n); __DSB(); } while (0)

int main(void)
{
  extern uint32_t g_pfnVectors;
  SCB->VTOR = (uint32_t)&g_pfnVectors;
  HAL_Init();
  SystemClock_Config();
  SysTick->LOAD  = (SystemCoreClock / 1000U) - 1UL;
  SysTick->VAL   = 0UL;
  SysTick->CTRL  = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  memset((void *)&shared_data_inst, 0, sizeof(shared_data_inst));
  shared_data_inst.magic = 0x53485244U;
  shared_data_inst.version = 2;
  shared_data_inst.cm4_write_seq = 1;
  __DSB();
  MX_GPIO_Init();
  WT_STAGE(1);
  MX_TIM2_Init();
  WT_STAGE(2);
  for(int j=0;j<2;j++){ LED_GREEN_ON(); LED_RED_ON(); DelayMs(20); LED_GREEN_OFF(); LED_RED_OFF(); DelayMs(20); }
  WT_STAGE(3);
  MX_SPI1_Init();
  WT_STAGE(4);
  LED_RED_ON();
  {
    uint8_t flush[N_JOINTS]={0};
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, flush, N_JOINTS, 100);
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);
  }
  WT_STAGE(5);
  HAL_GPIO_WritePin(DRV_RESET_GPIO_Port, DRV_RESET_Pin, GPIO_PIN_RESET);
  DelayMs(1);
  HAL_GPIO_WritePin(DRV_RESET_GPIO_Port, DRV_RESET_Pin, GPIO_PIN_SET);
  DelayMs(100);
  WT_STAGE(6);
  LED_RED_OFF();
  LED_GREEN_ON();
  RARM_SetBank(&bank);
  ps01Init(ps01_baremetal_get_os());
  // Use tuned setups from reference board (FS20000 int 0x1241/0xD69 etc.)
  runSetups();
  WT_STAGE(7);

  // All joints stay energised and holding; no HiZ anywhere. J2/J3 brakes are
  // released so those joints can move when their turn comes (and re-held by
  // the motor's hold voltage in between).
  RARM_ReleaseBrake(J2_INDEX);
  RARM_ReleaseBrake(J3_INDEX);
  memset(rr_faults,0,sizeof(rr_faults));
  shared_data_inst.fault_alarm=0;
  shared_data_inst.last_fault_code=0;
  shared_data_inst.last_fault_tick=0;
  mot_bank->active=J1_INDEX;
  shared_data_inst.motor_ready=1;
  shared_data_inst.motor_ready_seq=1;
  __DSB();
  DelayMs(2000);
  DelayMs(START_DELAY_MS);
  test_timer = DWT->CYCCNT / (SystemCoreClock / 1000UL);
  test_phase = 0;
  test_joint = J1_INDEX;
  uint32_t const cyccnt_1ms = SystemCoreClock / 1000UL;
  uint32_t last_cyc = DWT->CYCCNT;
  while(1){
    uint32_t now_cyc = DWT->CYCCNT;
    if((now_cyc - last_cyc) >= (cyccnt_1ms * MOTOR_LOOP_PERIOD_MS)){
      last_cyc = now_cyc;
      uint32_t now_ms = now_cyc / cyccnt_1ms;
      // J2/J3 brakes released for the whole run so those joints can move.
      RARM_ReleaseBrake(J2_INDEX);
      RARM_ReleaseBrake(J3_INDEX);

      uint8_t j = (test_joint < N_JOINTS) ? test_joint : 0U;
      uint8_t moving = RARM_IsMoving(j) ? 1U : 0U;
      switch(test_phase){
        case 0: /* clear latched faults, then command +STEP on this joint only */
          for(uint8_t i=0;i<N_JOINTS;i++){
            mot_bank->active=i;
            ps01SetAlarms_chain(ALARM_STALL|ALARM_CMD_ERR|ALARM_OVC|ALARM_THRM_SD|ALARM_THRM_WARN|ALARM_UVLO|ALARM_ADC_UVLO|ALARM_SW_EVENT);
            ps01GetStatus_chain();
          }
          RARM_MoveDegrees(j, STEP_DEG);
          test_timer = now_ms;
          test_phase = 1;
          break;
        case 1: /* wait for +STEP to finish (min 100 ms, timeout guards a stall) */
          if(((now_ms - test_timer) >= 100U) &&
             (!moving || (now_ms - test_timer) >= MOVE_TIMEOUT_MS)){
            test_timer = now_ms;
            test_phase = 2;
          }
          break;
        case 2: /* dwell at +STEP */
          if((now_ms - test_timer) >= SETTLE_MS){
            RARM_MoveDegrees(j, -STEP_DEG);
            test_timer = now_ms;
            test_phase = 3;
          }
          break;
        case 3: /* wait for the return move */
          if(((now_ms - test_timer) >= 100U) &&
             (!moving || (now_ms - test_timer) >= MOVE_TIMEOUT_MS)){
            test_timer = now_ms;
            test_phase = 4;
          }
          break;
        case 4: /* dwell, then hand over to the next joint */
          if((now_ms - test_timer) >= SETTLE_MS){
            test_joint = (uint8_t)((j + 1U) % N_JOINTS);
            test_phase = 0;
          }
          break;
        default:
          test_phase = 0;
          break;
      }
      Motor_UpdatePositions();
      uint8_t btn = HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin);
      if(btn_prev==1 && btn==0 && (now_ms - btn_last_ms)>50){
        btn_last_ms=now_ms;
        uint8_t found=0;
        for(uint8_t i=0;i<N_JOINTS;i++){
          mot_bank->active=i;
          uint16_t s=ps01GetStatus_chain();
          uint8_t alarm=RR_AlarmByte(s);
          shared_data_inst.fault_alarm &= ~((uint32_t)0xFF << (i*8U));
          if(alarm){ shared_data_inst.fault_alarm |= (uint32_t)alarm << (i*8U); rr_faults[i]=alarm; found=1; } else rr_faults[i]=0;
        }
        if(found){
          rr_joint=RR_NextFaulted(0);
          if(rr_joint<N_JOINTS){ rr_blinks=0; rr_total=RR_BlinkCount(rr_faults[rr_joint]); if(rr_total==0) rr_total=1; rr_phase=0; rr_pause=0; rr_timer=now_ms; LED_RED_ON(); }
        } else { btn_green_blinks=3; btn_green_phase=0; btn_green_timer=now_ms; LED_GREEN_ON(); }
        for(uint8_t i=0;i<N_JOINTS;i++){ mot_bank->active=i; ps01SetAlarms_chain(ALARM_STALL|ALARM_CMD_ERR|ALARM_OVC|ALARM_THRM_SD|ALARM_THRM_WARN|ALARM_UVLO); }
      }
      btn_prev=btn;
      RR_Tick(now_ms);
      HAL_HSEM_FastTake(HSEM_ID_SENSOR);
      HAL_HSEM_Release(HSEM_ID_SENSOR,0);
    }
  }
}
static void Motor_UpdatePositions(void){ for(uint8_t i=0;i<N_JOINTS;i++) shared_data_inst.joint_act_positions[i]=RARM_GetPositionMilliDegrees(i); shared_data_inst.motion_done_seq++; __DSB(); }
static void RR_PollFaults(void){ uint32_t any=0; for(uint8_t i=0;i<N_JOINTS;i++){ mot_bank->active=i; uint16_t s=ps01GetStatus_chain(); shared_data_inst.dbg_joint_status[i]=s; uint8_t a=RR_AlarmByte(s); shared_data_inst.fault_alarm &= ~((uint32_t)0xFF << (i*8U)); if(a){ any=1; rr_faults[i]=a; shared_data_inst.fault_alarm |= (uint32_t)a << (i*8U); ps01SetAlarms_chain(ALARM_STALL|ALARM_CMD_ERR|ALARM_OVC|ALARM_THRM_SD|ALARM_THRM_WARN|ALARM_UVLO);} else rr_faults[i]=0; } if(any && shared_data_inst.last_fault_code==0){ shared_data_inst.last_fault_code=shared_data_inst.fault_alarm; shared_data_inst.last_fault_tick=HAL_GetTick(); } }
static uint8_t RR_BlinkCount(uint16_t status){ PS01Status_t s={.reg=status}; if(s.bits.OCD) return 1; if(s.bits.TH_STATUS) return 2; if(s.bits.UVLO||s.bits.UVLO_ADC) return 3; if(s.bits.STALL_B||s.bits.STALL_A) return 4; if(s.bits.SW_EVN) return 5; if(s.bits.CMD_ERROR) return 6; return 0; }
static uint8_t RR_AlarmByte(uint16_t status){ PS01Status_t s={.reg=status}; uint8_t a=0; if(s.bits.OCD) a|=ALARM_OVC; if(s.bits.TH_STATUS==2) a|=ALARM_THRM_SD; if(s.bits.TH_STATUS==1) a|=ALARM_THRM_WARN; if(s.bits.UVLO) a|=ALARM_UVLO; if(s.bits.UVLO_ADC) a|=ALARM_ADC_UVLO; if(s.bits.STALL_B||s.bits.STALL_A) a|=ALARM_STALL; if(s.bits.SW_EVN) a|=ALARM_SW_EVENT; if(s.bits.CMD_ERROR) a|=ALARM_CMD_ERR; return a; }
static uint8_t RR_NextFaulted(uint8_t start){ for(uint8_t i=0;i<N_JOINTS;i++){ uint8_t idx=(start+i)%N_JOINTS; if(rr_faults[idx]) return idx; } return 0xFF; }
static void RR_Tick(uint32_t now){ RR_PollFaults(); if(btn_green_blinks){ if((now-btn_green_timer)>=100){ btn_green_timer=now; if(btn_green_phase==0){ LED_GREEN_OFF(); btn_green_phase=1;} else { LED_GREEN_ON(); if(--btn_green_blinks==0){ LED_GREEN_OFF(); btn_green_phase=0;} else btn_green_phase=0; } } return; } if((now-rr_timer)<120) return; rr_timer=now; if(rr_joint>=N_JOINTS || rr_faults[rr_joint]==0){ rr_joint=RR_NextFaulted(rr_joint+1); if(rr_joint>=N_JOINTS){ LED_RED_OFF(); return;} rr_blinks=0; rr_total=RR_BlinkCount(rr_faults[rr_joint]); if(rr_total==0) rr_total=1; rr_phase=0; rr_pause=0; LED_RED_ON(); return;} switch(rr_phase){ case 0: LED_RED_OFF(); rr_phase=1; break; case 1: rr_blinks++; if(rr_blinks>=rr_total){ rr_phase=2; rr_pause=5;} else { rr_phase=0; LED_RED_ON(); } break; case 2: if(--rr_pause==0){ uint8_t n=RR_NextFaulted(rr_joint+1); if(n>=N_JOINTS||n==rr_joint) n=RR_NextFaulted(0); rr_joint=(n>=N_JOINTS)?0xFF:n; rr_blinks=0; rr_phase=0; rr_pause=0; if(rr_joint<N_JOINTS){ rr_total=RR_BlinkCount(rr_faults[rr_joint]); if(rr_total==0) rr_total=1; LED_RED_ON(); } } break; } }
static void SystemClock_Config(void){ RCC_OscInitTypeDef RCC_OscInitStruct={0}; RCC_ClkInitTypeDef RCC_ClkInitStruct={0}; HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY); __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2); while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)); RCC_OscInitStruct.OscillatorType=RCC_OSCILLATORTYPE_HSI; RCC_OscInitStruct.HSIState=RCC_HSI_DIV1; RCC_OscInitStruct.HSICalibrationValue=RCC_HSICALIBRATION_DEFAULT; RCC_OscInitStruct.PLL.PLLState=RCC_PLL_ON; RCC_OscInitStruct.PLL.PLLSource=RCC_PLLSOURCE_HSI; RCC_OscInitStruct.PLL.PLLM=4; RCC_OscInitStruct.PLL.PLLN=28; RCC_OscInitStruct.PLL.PLLP=2; RCC_OscInitStruct.PLL.PLLQ=5; RCC_OscInitStruct.PLL.PLLR=2; RCC_OscInitStruct.PLL.PLLRGE=RCC_PLL1VCIRANGE_3; RCC_OscInitStruct.PLL.PLLVCOSEL=RCC_PLL1VCOWIDE; RCC_OscInitStruct.PLL.PLLFRACN=1024; HAL_RCC_OscConfig(&RCC_OscInitStruct); RCC_ClkInitStruct.ClockType=RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1; RCC_ClkInitStruct.SYSCLKSource=RCC_SYSCLKSOURCE_PLLCLK; RCC_ClkInitStruct.SYSCLKDivider=RCC_SYSCLK_DIV1; RCC_ClkInitStruct.AHBCLKDivider=RCC_HCLK_DIV1; RCC_ClkInitStruct.APB3CLKDivider=RCC_APB3_DIV2; RCC_ClkInitStruct.APB1CLKDivider=RCC_APB1_DIV2; RCC_ClkInitStruct.APB2CLKDivider=RCC_APB2_DIV2; RCC_ClkInitStruct.APB4CLKDivider=RCC_APB4_DIV2; HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4); }
void Error_Handler(void){ __disable_irq(); while(1){ LED_RED_ON(); DelayMs(100); LED_RED_OFF(); DelayMs(100); } }
