#ifndef __PS01_H__
#define __PS01_H__

#include "main.h"
#include "stm32h7xx_hal.h"
#include "spi.h"
#include "cfg_structs.h"
#include "ps01calc.h"

extern SPI_HandleTypeDef hspi1;

typedef struct
{
    void *mutex;
    void (*mutex_acquire)(void *mutex);
    void (*mutex_release)(void *mutex);
    void (*delay_ms)(uint32_t ms);
} PS01_OS_t;

extern StepperBank_t *mot_bank;
extern uint8_t MOT_NUMBER;

void ps01SetBank                   (StepperBank_t *bank, uint8_t n_motors);
void ps01Init                      (const PS01_OS_t *os);

uint16_t ps01GetStatus_chain       ();

void     ps01SetParam_chain        (uint8_t param, uint32_t value);
uint32_t ps01GetParam_chain        (uint8_t param);

void ps01SetConfig_chain           ();
void ps01SetStepMode_chain         ();
void ps01SetKVALs_chain            ();
void ps01SetMaxSpeed_chain         (uint16_t steps_s);
void ps01SetMinSpeed_chain         (uint16_t steps_s);
void ps01SetAcceleration_chain     (uint16_t steps_s2);
void ps01SetDeceleration_chain     (uint16_t steps_s2);
void ps01SetFullStepSpeed_chain    (uint16_t steps_s);
void ps01SetOcThreshold_chain      (float trip_amps);
void ps01SetStallThreshold_chain   (float trip_amps);
void ps01SetAlarms_chain           (uint8_t alarm_bits);

void ps01WaitBusy_chain            ();

int32_t ps01GetPosition_chain      ();
int32_t ps01GetPositionDegrees_chain ();

void ps01Run_chain                 (uint8_t dir, uint16_t steps_s);
void ps01StepClock_chain           (uint8_t dir);
void ps01Move_chain                (uint8_t dir, uint32_t n_steps);
void ps01MoveDegrees_chain         (uint8_t dir, uint16_t deg);
void ps01GoTo_chain                (int32_t abs_pos);
void ps01GoToDegrees_chain         (int32_t abs_deg);
void ps01GoTo_DIR_chain            (uint8_t dir, int32_t abs_pos);
void ps01GoToDegrees_DIR_chain     (uint8_t dir, int32_t abs_deg);
void ps01GoUntil_chain             (uint8_t act, uint8_t dir, uint16_t steps_s);
void ps01ReleaseSW_chain           (uint8_t act, uint8_t dir);
void ps01GoHome_chain              ();
void ps01GoMark_chain              ();
void ps01ResetPos_chain            ();
void ps01ResetDevice_chain         ();
void ps01SoftStop_chain            ();
void ps01HardStop_chain            ();
void ps01SoftHiZ_chain             ();
void ps01HardHiZ_chain            ();

#endif
