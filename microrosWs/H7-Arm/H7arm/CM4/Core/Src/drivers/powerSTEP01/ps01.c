#include "ps01.h"
#include "cfg_structs.h"
#include "ps01calc.h"
#include "stm32h7xx_hal.h"
#include <string.h>

#define PS01_DAISY_MAX  10U
#define PS01_SPI_GUARD_MS 100U

static const PS01_OS_t *ps01_os;
static volatile uint8_t ps01_spi_busy = 0U;

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

static void ps01_acquire_spi(void)
{
    uint32_t start = HAL_GetTick();
    while (ps01_spi_busy) {
        if ((HAL_GetTick() - start) > PS01_SPI_GUARD_MS) {
            ps01_spi_busy = 0U;
            break;
        }
    }
    ps01_spi_busy = 1U;
    __DSB();
}

static void ps01_release_spi(void)
{
    __DSB();
    ps01_spi_busy = 0U;
}

void ps01_write_byte(uint8_t byte)
{
    uint8_t tx[PS01_DAISY_MAX];
    uint8_t n = (MOT_NUMBER < PS01_DAISY_MAX) ? MOT_NUMBER : PS01_DAISY_MAX;

    memset(tx, 0, n);
    tx[n - 1U - mot_bank->active] = byte;

    ps01_acquire_spi();

    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, tx, n, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);

    ps01_release_spi();

    if (ps01_os != NULL) {
        ps01_os->delay_ms(1U);
    }
}

uint8_t ps01_read_byte(void)
{
    uint8_t tx[PS01_DAISY_MAX];
    uint8_t rx[PS01_DAISY_MAX];
    uint8_t n  = (MOT_NUMBER < PS01_DAISY_MAX) ? MOT_NUMBER : PS01_DAISY_MAX;
    uint8_t rc = 0U;

    memset(tx, 0, n);
    memset(rx, 0, n);

    ps01_acquire_spi();

    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, n, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);

    ps01_release_spi();

    if (ps01_os != NULL) {
        ps01_os->delay_ms(1U);
    }

    rc = rx[n - 1U - mot_bank->active];
    return rc;
}

void ps01_xfer_bits(uint32_t value, uint8_t bitlen)
{
    uint8_t nbytes = bitlen / 8U;
    if ((bitlen % 8U) != 0U) nbytes++;

    for (uint8_t i = 0; i < nbytes; i++) {
        ps01_write_byte((uint8_t)(value >> ((nbytes - i - 1U) * 8U)));
    }
}

uint32_t ps01_rx_bits(uint8_t bitlen)
{
    uint32_t retval = 0U;
    uint8_t  nbytes = bitlen / 8U;
    if ((bitlen % 8U) != 0U) nbytes++;

    for (uint8_t i = 0; i < nbytes; i++) {
        retval <<= 8;
        retval |= ps01_read_byte();
    }

    return retval;
}

uint16_t ps01GetStatus_chain(void)
{
    ps01_write_byte(0xD0);
    return (uint16_t)ps01_rx_bits(16);
}

void ps01SetParam_chain(uint8_t param, uint32_t value)
{
    ps01_write_byte(param);

    switch (param) {
        case ABS_POS:    ps01_xfer_bits(value, 22); break;
        case EL_POS:     ps01_xfer_bits(value,  9); break;
        case MARK:       ps01_xfer_bits(value, 22); break;
        case ACC:        ps01_xfer_bits(value, 12); break;
        case DEC:        ps01_xfer_bits(value, 12); break;
        case MAX_SPEED:  ps01_xfer_bits(value, 10); break;
        case MIN_SPEED:  ps01_xfer_bits(value, 12); break;
        case OCD_TH:     ps01_xfer_bits(value,  5); break;
        case FS_SPD:     ps01_xfer_bits(value, 11); break;
        case STEP_MODE:  ps01_xfer_bits(value,  8); break;
        case ALARM_EN:   ps01_xfer_bits(value,  8); break;
        case GATECFG1:   ps01_xfer_bits(value, 11); break;
        case GATECFG2:   ps01_xfer_bits(value,  8); break;
        case CONFIG:     ps01_xfer_bits(value, 16); break;
        case KVAL_HOLD:  ps01_xfer_bits(value,  8); break;
        case KVAL_RUN:   ps01_xfer_bits(value,  8); break;
        case KVAL_ACC:   ps01_xfer_bits(value,  8); break;
        case KVAL_DEC:   ps01_xfer_bits(value,  8); break;
        case INT_SPEED:  ps01_xfer_bits(value, 14); break;
        case ST_SLP:     ps01_xfer_bits(value,  8); break;
        case FN_SLP_ACC: ps01_xfer_bits(value,  8); break;
        case FN_SLP_DEC: ps01_xfer_bits(value,  8); break;
        case K_THERM:    ps01_xfer_bits(value,  4); break;
        case STALL_TH:   ps01_xfer_bits(value,  5); break;
        default: break;
    }
}

uint32_t ps01GetParam_chain(uint8_t param)
{
    ps01_write_byte(0x20 | param);
    switch (param) {
        case ABS_POS:    return ps01_rx_bits(22);
        case EL_POS:     return ps01_rx_bits( 9);
        case MARK:       return ps01_rx_bits(22);
        case SPEED:      return ps01_rx_bits(20);
        case ACC:        return ps01_rx_bits(12);
        case DEC:        return ps01_rx_bits(12);
        case MAX_SPEED:  return ps01_rx_bits(10);
        case MIN_SPEED:  return ps01_rx_bits(12);
        case ADC_OUT:    return ps01_rx_bits( 5);
        case OCD_TH:     return ps01_rx_bits( 5);
        case FS_SPD:     return ps01_rx_bits(11);
        case STEP_MODE:  return ps01_rx_bits( 8);
        case ALARM_EN:   return ps01_rx_bits( 8);
        case GATECFG1:   return ps01_rx_bits(11);
        case GATECFG2:   return ps01_rx_bits( 8);
        case CONFIG:     return ps01_rx_bits(16);
        case KVAL_HOLD:  return ps01_rx_bits( 8);
        case KVAL_RUN:   return ps01_rx_bits( 8);
        case KVAL_ACC:   return ps01_rx_bits( 8);
        case KVAL_DEC:   return ps01_rx_bits( 8);
        case INT_SPEED:  return ps01_rx_bits(14);
        case ST_SLP:     return ps01_rx_bits( 8);
        case FN_SLP_ACC: return ps01_rx_bits( 8);
        case FN_SLP_DEC: return ps01_rx_bits( 8);
        case K_THERM:    return ps01_rx_bits( 4);
        case STALL_TH:   return ps01_rx_bits( 5);
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

/* TODO(blocked on electronics): confirm Rds(ON) — currently uses HOT (125°C) value.
 * The function comment in the original code states this should be a mean value
 * between 25°C and 125°C; awaiting confirmation from the electronics team. */
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
    ps01_write_byte(RUN_CMD | (dir & 0x01U));
    ps01_write_byte((uint8_t)(spd >> 16));
    ps01_write_byte((uint8_t)(spd >>  8));
    ps01_write_byte((uint8_t)spd);
}

void ps01StepClock_chain(uint8_t dir)
{
    ps01_write_byte(STEPCLK_CMD | (dir & 0x01U));
}

void ps01Move_chain(uint8_t dir, uint32_t n_steps)
{
    ps01_write_byte(MOVE_CMD | (dir & 0x01U));
    ps01_write_byte((uint8_t)(n_steps >> 16));
    ps01_write_byte((uint8_t)(n_steps >>  8));
    ps01_write_byte((uint8_t)n_steps);
}

void ps01MoveDegrees_chain(uint8_t dir, uint16_t deg)
{
    ps01Move_chain(dir, (uint32_t)getStepsFromAngle((int32_t)deg, &mot_bank->motors[mot_bank->active]));
}

void ps01GoTo_chain(int32_t abs_pos)
{
    ps01_write_byte(GOTO_CMD);
    ps01_write_byte((uint8_t)(abs_pos >> 16));
    ps01_write_byte((uint8_t)(abs_pos >>  8));
    ps01_write_byte((uint8_t)abs_pos);
}

void ps01GoToDegrees_chain(int32_t abs_deg)
{
    int32_t pos = getStepsFromAngle(abs_deg, &mot_bank->motors[mot_bank->active]);
    ps01GoTo_chain(pos);
}

void ps01GoTo_DIR_chain(uint8_t dir, int32_t abs_pos)
{
    ps01_write_byte(GOTO_DIR_CMD | (dir & 0x01U));
    ps01_write_byte((uint8_t)(abs_pos >> 16));
    ps01_write_byte((uint8_t)(abs_pos >>  8));
    ps01_write_byte((uint8_t)abs_pos);
}

void ps01GoToDegrees_DIR_chain(uint8_t dir, int32_t abs_deg)
{
    int32_t pos = getStepsFromAngle(abs_deg, &mot_bank->motors[mot_bank->active]);
    ps01GoTo_DIR_chain(dir, pos);
}

void ps01GoUntil_chain(uint8_t act, uint8_t dir, uint16_t steps_s)
{
    uint32_t spd = calculateSpeed(steps_s);
    ps01_write_byte(GOUNTIL_CMD | ((act & 0x01U) << 3) | (dir & 0x01U));
    ps01_write_byte((uint8_t)(spd >> 16));
    ps01_write_byte((uint8_t)(spd >>  8));
    ps01_write_byte((uint8_t)spd);
}

void ps01ReleaseSW_chain(uint8_t act, uint8_t dir)
{
    ps01_write_byte(RELEASESW_CMD | ((act & 0x01U) << 3) | (dir & 0x01U));
}

void ps01GoHome_chain(void)     { ps01_write_byte(GOHOME_CMD); }
void ps01GoMark_chain(void)     { ps01_write_byte(GOMARK_CMD); }
void ps01ResetPos_chain(void)   { ps01_write_byte(RESETPOS_CMD); }
void ps01ResetDevice_chain(void){ ps01_write_byte(RESETDEV_CMD); }
void ps01SoftStop_chain(void)   { ps01_write_byte(SOFTSTOP_CMD); }
void ps01HardStop_chain(void)   { ps01_write_byte(HARDSTOP_CMD); }
void ps01SoftHiZ_chain(void)    { ps01_write_byte(SOFTHIZ_CMD); }
void ps01HardHiZ_chain(void)    { ps01_write_byte(HARDHIZ_CMD);  }

int32_t ps01GetPositionDegrees_chain(void)
{
    int32_t  steps        = ps01GetPosition_chain();
    int64_t  steps_per_rev = mot_bank->motors[mot_bank->active].steps_rev;
    int64_t  ratio         = mot_bank->motors[mot_bank->active].reduction_ratio;
    int64_t  microsteps    = mot_bank->motors[mot_bank->active].stepmode.bits.STEP_SEL;
    return (int32_t)((int64_t)steps * 360 / (steps_per_rev << microsteps) / ratio);
}
