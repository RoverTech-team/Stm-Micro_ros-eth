#ifndef PS01_H
#define PS01_H

#include "main.h"
#include "stm32h7xx_hal.h"
#include "spi.h"
#include "cfg_structs.h"
#include "ps01calc.h"

typedef struct
{
    void  *semaphore;
    void (*semaphore_acquire)(void *sem);
    void  (*delay_ms)(uint32_t ms);
} PS01_OS_t;

void ps01SetBank  (StepperBank_t *bank, uint8_t n_motors);
void ps01Init     (const PS01_OS_t *os);

extern StepperBank_t *mot_bank;
extern uint8_t        MOT_NUMBER;

uint16_t ps01GetStatus_chain      (void);
void     ps01SetParam_chain       (uint8_t param, uint32_t value);
uint32_t ps01GetParam_chain       (uint8_t param);

void ps01SetConfig_chain          (void);
void ps01SetStepMode_chain        (void);
void ps01SetKVALs_chain           (void);
void ps01SetMaxSpeed_chain        (uint16_t steps_s);
void ps01SetMinSpeed_chain        (uint16_t steps_s);
void ps01SetAcceleration_chain    (uint16_t steps_s2);
void ps01SetDeceleration_chain    (uint16_t steps_s2);
void ps01SetFullStepSpeed_chain   (uint16_t steps_s);
void ps01SetOcThreshold_chain     (float trip_amps);
void ps01SetStallThreshold_chain  (float trip_amps);
void ps01SetAlarms_chain          (uint8_t alarm_bits);

void ps01WaitBusy_chain           (void);

int32_t ps01GetPosition_chain     (void);
int32_t ps01GetPositionDegrees_chain(void);

void ps01Run_chain                (uint8_t dir, uint16_t steps_s);
void ps01StepClock_chain          (uint8_t dir);
void ps01Move_chain               (uint8_t dir, uint32_t n_steps);
void ps01MoveDegrees_chain        (uint8_t dir, uint16_t deg);
void ps01GoTo_chain               (int32_t abs_pos);
void ps01GoToDegrees_chain        (int32_t abs_deg);
void ps01GoTo_DIR_chain           (uint8_t dir, int32_t abs_pos);
void ps01GoToDegrees_DIR_chain    (uint8_t dir, int32_t abs_deg);
void ps01GoUntil_chain            (uint8_t act, uint8_t dir, uint16_t steps_s);
void ps01ReleaseSW_chain          (uint8_t act, uint8_t dir);
void ps01GoHome_chain             (void);
void ps01GoMark_chain             (void);
void ps01ResetPos_chain           (void);
void ps01ResetDevice_chain        (void);
void ps01SoftStop_chain           (void);
void ps01HardStop_chain           (void);
void ps01SoftHiZ_chain            (void);
void ps01HardHiZ_chain            (void);

void     ps01_write_byte          (uint8_t byte);
uint8_t  ps01_read_byte           (void);
void     ps01_xfer_bits           (uint32_t value, uint8_t bitlen);
uint32_t ps01_rx_bits             (uint8_t bitlen);

#endif
