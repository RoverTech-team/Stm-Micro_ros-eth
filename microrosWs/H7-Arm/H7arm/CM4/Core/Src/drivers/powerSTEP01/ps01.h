#ifndef __PS01_H__
#define __PS01_H__

#include "main.h"
#include "stm32h7xx_hal.h"
#include "cfg_structs.h"
#include "ps01calc.h"

/* SPI handle — defined in main.c */
extern SPI_HandleTypeDef hspi1;

/* ------------------------------------------------------------------ */
/*  OS abstraction – replaces the former cmsis_os2.h dependency       */
/*                                                                    */
/*  The driver no longer includes any RTOS header.  Instead, the      */
/*  application must provide an implementation of PS01_OS_t and       */
/*  register it with ps01Init() before using any other API.           */
/*                                                                    */
/*  This makes the driver portable across bare-metal, CMSIS-RTOS2,    */
/*  FreeRTOS, or any other environment.                               */
/* ------------------------------------------------------------------ */

typedef struct
{
    void *mutex;                           /**< Opaque SPI-mutex handle        */
    void *semaphore;                       /**< Opaque SPI-DMA semaphore handle*/
    void (*mutex_acquire)(void *mutex);    /**< Block until mutex is owned     */
    void (*mutex_release)(void *mutex);    /**< Release a previously held mutex*/
    void (*semaphore_acquire)(void *sem);  /**< Block until semaphore is avail.*/
    void (*delay_ms)(uint32_t ms);         /**< Millisecond delay (may yield)  */
} PS01_OS_t;

extern StepperBank_t *mot_bank;
extern uint8_t MOT_NUMBER;

/* ---- Initialisation ------------------------------------------------ */

void ps01SetBank                   (StepperBank_t *bank, uint8_t n_motors);
void ps01Init                      (const PS01_OS_t *os);  /**< must be called first */

/* ---- Register / status access -------------------------------------- */

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

/* ---- Motion commands ----------------------------------------------- */

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
void ps01HardHiZ_chain             ();

/* ---- Low-level SPI helpers ----------------------------------------- */

void     _writebyte_chain          (uint8_t byte);
uint8_t  _readbyte_chain           ();
void     _xferbits_chain           (uint32_t value, uint8_t bitlen);
uint32_t _rxbits_chain             (uint8_t bitlen);

#endif
