#include "ps01.h"
#include "cfg_structs.h"
#include "ps01calc.h"
#include "stm32h7xx_hal.h"
#include "spi.h"
#include "shared_data.h"
#include <string.h>

#define PS01_DAISY_MAX  10U

static const PS01_OS_t *ps01_os;

StepperBank_t *mot_bank;
uint8_t        MOT_NUMBER;

void ps01SetBank(StepperBank_t *bank, uint8_t n_motors)
{
    mot_bank   = bank;
    MOT_NUMBER = n_motors;
}

void ps01Init(const PS01_OS_t *os)
{
    ps01_os = os;
}

static void _writebyte_chain(uint8_t byte)
{
    uint8_t tx[MOT_NUMBER];
    uint8_t rx[MOT_NUMBER];

    memset(tx, 0x00U, sizeof(tx));
    memset(rx, 0x00U, sizeof(rx));
    tx[MOT_NUMBER - 1U - mot_bank->active] = byte;

    SHARED_DATA->last_fault_cfb++;
    ps01_os->mutex_acquire(ps01_os->mutex);
    SHARED_DATA->last_fault_cfb++;

    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_RESET);
    SHARED_DATA->last_fault_cfb++;
    for (int retry = 0; retry < 3; retry++) {
        if (SPI1_Transfer(tx, rx, MOT_NUMBER) == 0)
            break;
        HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);
        for (volatile uint32_t d = 0; d < 20000; d++) { ; }
        HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_RESET);
    }
    SHARED_DATA->last_fault_cfb++;
    SHARED_DATA->last_fault_cfb++;
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);
    SHARED_DATA->last_fault_cfb++;

    ps01_os->mutex_release(ps01_os->mutex);
    SHARED_DATA->last_fault_cfb++;

    if (ps01_os != NULL) {
        for (volatile uint32_t d = 0; d < 8000; d++) { ; }
    }
    SHARED_DATA->last_fault_cfb++;
}

static uint8_t _readbyte_chain(void)
{
    uint8_t tx[MOT_NUMBER];
    uint8_t rx[MOT_NUMBER];

    memset(tx, 0, sizeof(tx));
    memset(rx, 0, sizeof(rx));

    ps01_os->mutex_acquire(ps01_os->mutex);

    SHARED_DATA->last_fault_cfb++;
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_RESET);
    SHARED_DATA->last_fault_cfb++;
    for (int retry = 0; retry < 3; retry++) {
        if (SPI1_Transfer(tx, rx, MOT_NUMBER) == 0)
            break;
        HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);
        for (volatile uint32_t d = 0; d < 20000; d++) { ; }
        HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_RESET);
    }
    SHARED_DATA->last_fault_cfb++;
    SHARED_DATA->last_fault_cfb++;
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);
    SHARED_DATA->last_fault_cfb++;

    ps01_os->mutex_release(ps01_os->mutex);
    SHARED_DATA->last_fault_cfb++;

    if (ps01_os != NULL) {
        for (volatile uint32_t d = 0; d < 8000; d++) { ; }
    }
    SHARED_DATA->last_fault_cfb++;

    return rx[MOT_NUMBER - 1U - mot_bank->active];
}



static void _xferbits_chain(uint32_t value, uint8_t bitlen)
{
    uint8_t nbytes = bitlen / 8U;
    if ((bitlen % 8U) != 0U) nbytes++;

    for (uint8_t i = 0; i < nbytes; i++) {
        _writebyte_chain((uint8_t)(value >> ((nbytes - i - 1U) * 8U)));
    }
}

static uint32_t _rxbits_chain(uint8_t bitlen)
{
    uint32_t retval = 0U;
    uint8_t  nbytes = bitlen / 8U;
    if ((bitlen % 8U) != 0U) nbytes++;

    for (uint8_t i = 0; i < nbytes; i++) {
        retval <<= 8;
        retval |= _readbyte_chain();
    }

    return retval;
}

uint16_t ps01GetStatus_chain(void)
{
    _writebyte_chain(0xD0);
    return (uint16_t)_rxbits_chain(16);
}

void ps01SetParam_chain(uint8_t param, uint32_t value)
{
    _writebyte_chain(param);

    switch (param) {
        case ABS_POS:    _xferbits_chain(value, 22); break;
        case EL_POS:     _xferbits_chain(value,  9); break;
        case MARK:       _xferbits_chain(value, 22); break;
        case ACC:        _xferbits_chain(value, 12); break;
        case DEC:        _xferbits_chain(value, 12); break;
        case MAX_SPEED:  _xferbits_chain(value, 10); break;
        case MIN_SPEED:  _xferbits_chain(value, 12); break;
        case OCD_TH:     _xferbits_chain(value,  5); break;
        case FS_SPD:     _xferbits_chain(value, 11); break;
        case STEP_MODE:  _xferbits_chain(value,  8); break;
        case ALARM_EN:   _xferbits_chain(value,  8); break;
        case GATECFG1:   _xferbits_chain(value, 11); break;
        case GATECFG2:   _xferbits_chain(value,  8); break;
        case CONFIG:     _xferbits_chain(value, 16); break;
        case KVAL_HOLD:  _xferbits_chain(value,  8); break;
        case KVAL_RUN:   _xferbits_chain(value,  8); break;
        case KVAL_ACC:   _xferbits_chain(value,  8); break;
        case KVAL_DEC:   _xferbits_chain(value,  8); break;
        case INT_SPEED:  _xferbits_chain(value, 14); break;
        case ST_SLP:     _xferbits_chain(value,  8); break;
        case FN_SLP_ACC: _xferbits_chain(value,  8); break;
        case FN_SLP_DEC: _xferbits_chain(value,  8); break;
        case K_THERM:    _xferbits_chain(value,  4); break;
        case STALL_TH:   _xferbits_chain(value,  5); break;
        default: break;
    }
}

uint32_t ps01GetParam_chain(uint8_t param)
{
    _writebyte_chain(0x20 | param);
    switch (param) {
        case ABS_POS:    return _rxbits_chain(22);
        case EL_POS:     return _rxbits_chain( 9);
        case MARK:       return _rxbits_chain(22);
        case SPEED:      return _rxbits_chain(20);
        case ACC:        return _rxbits_chain(12);
        case DEC:        return _rxbits_chain(12);
        case MAX_SPEED:  return _rxbits_chain(10);
        case MIN_SPEED:  return _rxbits_chain(12);
        case ADC_OUT:    return _rxbits_chain( 5);
        case OCD_TH:     return _rxbits_chain( 5);
        case FS_SPD:     return _rxbits_chain(11);
        case STEP_MODE:  return _rxbits_chain( 8);
        case ALARM_EN:   return _rxbits_chain( 8);
        case GATECFG1:   return _rxbits_chain(11);
        case GATECFG2:   return _rxbits_chain( 8);
        case CONFIG:     return _rxbits_chain(16);
        case KVAL_HOLD:  return _rxbits_chain( 8);
        case KVAL_RUN:   return _rxbits_chain( 8);
        case KVAL_ACC:   return _rxbits_chain( 8);
        case KVAL_DEC:   return _rxbits_chain( 8);
        case INT_SPEED:  return _rxbits_chain(14);
        case ST_SLP:     return _rxbits_chain( 8);
        case FN_SLP_ACC: return _rxbits_chain( 8);
        case FN_SLP_DEC: return _rxbits_chain( 8);
        case K_THERM:    return _rxbits_chain( 4);
        case STALL_TH:   return _rxbits_chain( 5);
        default:         return 0U;
    }
}

void ps01SetConfig_chain(void)
{
    ps01SetParam_chain(CONFIG, mot_bank->motors[mot_bank->active].config.reg);
}

void ps01SetStepMode_chain(void)
{
    ps01SetParam_chain(STEP_MODE, mot_bank->motors[mot_bank->active].stepmode.reg);
}

void ps01SetKVALs_chain(void)
{
    ps01SetParam_chain(KVAL_ACC,  mot_bank->motors[mot_bank->active].kvals.acceleration);
    ps01SetParam_chain(KVAL_DEC,  mot_bank->motors[mot_bank->active].kvals.deceleration);
    ps01SetParam_chain(KVAL_RUN,  mot_bank->motors[mot_bank->active].kvals.run);
    ps01SetParam_chain(KVAL_HOLD, mot_bank->motors[mot_bank->active].kvals.hold);
}

void ps01SetMaxSpeed_chain(uint16_t steps_s)
{
    ps01SetParam_chain(MAX_SPEED, calculateMaxMinSpeed(steps_s));
}

void ps01SetMinSpeed_chain(uint16_t steps_s)
{
    ps01SetParam_chain(MIN_SPEED, calculateMaxMinSpeed(steps_s));
}

void ps01SetAcceleration_chain(uint16_t steps_s2)
{
    ps01SetParam_chain(ACC, calculateAcceleration(steps_s2));
}

void ps01SetDeceleration_chain(uint16_t steps_s2)
{
    ps01SetParam_chain(DEC, calculateAcceleration(steps_s2));
}

void ps01SetFullStepSpeed_chain(uint16_t steps_s)
{
    ps01SetParam_chain(FS_SPD, calculateMaxMinSpeed(steps_s));
}

void ps01SetOcThreshold_chain(float trip_amps)
{
    float    v_th  = trip_amps * PS01_RDS_ON_HOT_OHM;
    uint8_t  reg   = (uint8_t)(v_th / PS01_OCD_STEP_V);
    if (reg > PS01_OCD_REG_MAX) reg = PS01_OCD_REG_MAX;
    ps01SetParam_chain(OCD_TH, reg);
}

void ps01SetStallThreshold_chain(float trip_amps)
{
    float    v_th  = trip_amps * PS01_RDS_ON_HOT_OHM;
    uint8_t  reg   = (uint8_t)(v_th / PS01_OCD_STEP_V);
    if (reg > PS01_OCD_REG_MAX) reg = PS01_OCD_REG_MAX;
    ps01SetParam_chain(STALL_TH, reg);
}

void ps01SetAlarms_chain(uint8_t alarm_bits)
{
    ps01SetParam_chain(ALARM_EN, alarm_bits);
}

void ps01WaitBusy_chain(void)
{
    uint16_t status;
    do {
        ps01_os->delay_ms(10U);
        status = ps01GetParam_chain(STATUS);
    } while ((status & STATUS_BUSY_BIT) == 0U);
}

int32_t ps01GetPosition_chain(void)
{
    int32_t raw = (int32_t)ps01GetParam_chain(ABS_POS);
    return (raw << 10) >> 10;
}

void ps01Run_chain(uint8_t dir, uint16_t steps_s)
{
    uint32_t spd = calculateSpeed(steps_s);
    _writebyte_chain(RUN_CMD | (dir & 0x01U));
    _writebyte_chain((uint8_t)(spd >> 16));
    _writebyte_chain((uint8_t)(spd >>  8));
    _writebyte_chain((uint8_t)spd);
}

void ps01StepClock_chain(uint8_t dir)
{
    _writebyte_chain(STEPCLK_CMD | (dir & 0x01U));
}

void ps01Move_chain(uint8_t dir, uint32_t n_steps)
{
    _writebyte_chain(MOVE_CMD | (dir & 0x01U));
    _writebyte_chain((uint8_t)(n_steps >> 16));
    _writebyte_chain((uint8_t)(n_steps >>  8));
    _writebyte_chain((uint8_t)n_steps);
}

void ps01MoveDegrees_chain(uint8_t dir, uint16_t deg)
{
    ps01Move_chain(dir, (uint32_t)getStepsFromAngle((int32_t)deg, &mot_bank->motors[mot_bank->active]));
}

void ps01GoTo_chain(int32_t abs_pos)
{
    _writebyte_chain(GOTO_CMD);
    _writebyte_chain((uint8_t)(abs_pos >> 16));
    _writebyte_chain((uint8_t)(abs_pos >>  8));
    _writebyte_chain((uint8_t)abs_pos);
}

void ps01GoToDegrees_chain(int32_t abs_deg)
{
    int32_t pos = getStepsFromAngle(abs_deg, &mot_bank->motors[mot_bank->active]);
    ps01GoTo_chain(pos);
}

void ps01GoTo_DIR_chain(uint8_t dir, int32_t abs_pos)
{
    _writebyte_chain(GOTO_DIR_CMD | (dir & 0x01U));
    _writebyte_chain((uint8_t)(abs_pos >> 16));
    _writebyte_chain((uint8_t)(abs_pos >>  8));
    _writebyte_chain((uint8_t)abs_pos);
}

void ps01GoToDegrees_DIR_chain(uint8_t dir, int32_t abs_deg)
{
    int32_t pos = getStepsFromAngle(abs_deg, &mot_bank->motors[mot_bank->active]);
    ps01GoTo_DIR_chain(dir, pos);
}

void ps01GoUntil_chain(uint8_t act, uint8_t dir, uint16_t steps_s)
{
    uint32_t spd = calculateSpeed(steps_s);
    _writebyte_chain(GOUNTIL_CMD | ((act & 0x01U) << 3) | (dir & 0x01U));
    _writebyte_chain((uint8_t)(spd >> 16));
    _writebyte_chain((uint8_t)(spd >>  8));
    _writebyte_chain((uint8_t)spd);
}

void ps01ReleaseSW_chain(uint8_t act, uint8_t dir)
{
    _writebyte_chain(RELEASESW_CMD | ((act & 0x01U) << 3) | (dir & 0x01U));
}

void ps01GoHome_chain(void)     { _writebyte_chain(GOHOME_CMD); }
void ps01GoMark_chain(void)     { _writebyte_chain(GOMARK_CMD); }
void ps01ResetPos_chain(void)   { _writebyte_chain(RESETPOS_CMD); }
void ps01ResetDevice_chain(void){ _writebyte_chain(RESETDEV_CMD); }
void ps01SoftStop_chain(void)   { _writebyte_chain(SOFTSTOP_CMD); }
void ps01HardStop_chain(void)   { _writebyte_chain(HARDSTOP_CMD); }
void ps01SoftHiZ_chain(void)    { _writebyte_chain(SOFTHIZ_CMD); }
void ps01HardHiZ_chain(void)    { _writebyte_chain(HARDHIZ_CMD);  }

int32_t ps01GetPositionDegrees_chain(void)
{
    int32_t  steps        = ps01GetPosition_chain();
    int64_t  steps_per_rev = mot_bank->motors[mot_bank->active].steps_rev;
    int64_t  ratio         = mot_bank->motors[mot_bank->active].reduction_ratio;
    int64_t  microsteps    = mot_bank->motors[mot_bank->active].stepmode.bits.STEP_SEL;
    return (int32_t)((int64_t)steps * 360 / (steps_per_rev << microsteps) / ratio);
}
