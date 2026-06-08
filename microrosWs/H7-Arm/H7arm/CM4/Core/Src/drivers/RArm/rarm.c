#include "rarm.h"
#include "main.h"

void RARM_SetBank(StepperBank_t *bank)
{
    ps01SetBank(bank, N_JOINTS);
}

void RARM_SetConfig(uint8_t joint_index, RARM_SimpleConfig_t *config)
{
    if (joint_index >= N_JOINTS) return;

    mot_bank->active = joint_index;

    mot_bank->motors[joint_index].steps_rev       = config->steps_rev;
    mot_bank->motors[joint_index].reduction_ratio = config->reduction_ratio;
    mot_bank->motors[joint_index].min_degs        = config->min_degs;
    mot_bank->motors[joint_index].max_degs        = config->max_degs;

    ps01SetParam_chain(GATECFG1, 0x0C8);
    ps01SetParam_chain(GATECFG2, 0x41);

    mot_bank->motors[joint_index].config.bits.OSC_SEL    = 0;
    mot_bank->motors[joint_index].config.bits.EXT_CLK    = 0;
    mot_bank->motors[joint_index].config.bits.SW_MODE    = SW_DISABLED;
    mot_bank->motors[joint_index].config.bits.EN_VSCOMP  = config->VSCOMP;
    mot_bank->motors[joint_index].config.bits.OC_SD      = config->OVERCURRENT_SD;
    mot_bank->motors[joint_index].config.bits.UVLOVAL    = UVLOVAL_6V3;
    mot_bank->motors[joint_index].config.bits.VCCVAL     = VCC_7V5;
    mot_bank->motors[joint_index].config.bits.F_PWM_DEC  = 0b011;
    mot_bank->motors[joint_index].config.bits.F_PWM_INT  = 0b000;
    ps01SetConfig_chain();

    mot_bank->motors[joint_index].stepmode.bits.STEP_SEL = config->STEP_MODE;
    mot_bank->motors[joint_index].stepmode.bits.CM_VM    = VOLTAGE_MODE;
    mot_bank->motors[joint_index].stepmode.bits.SYNC_EN  = SYNC_DISABLED;
    mot_bank->motors[joint_index].stepmode.bits.SYNC_SEL = 0;
    ps01SetStepMode_chain();

    mot_bank->motors[joint_index].kvals.acceleration = calculateKVAL(config->supply_voltage, config->run_acc_dec_voltage);
    mot_bank->motors[joint_index].kvals.deceleration = calculateKVAL(config->supply_voltage, config->run_acc_dec_voltage);
    mot_bank->motors[joint_index].kvals.run          = calculateKVAL(config->supply_voltage, config->run_acc_dec_voltage);
    mot_bank->motors[joint_index].kvals.hold         = calculateKVAL(config->supply_voltage, config->hold_voltage);
    ps01SetKVALs_chain();

    ps01SetMaxSpeed_chain(config->max_speed);
    ps01SetMinSpeed_chain(config->min_speed);
    ps01SetAcceleration_chain(config->acceleration);
    ps01SetDeceleration_chain(config->deceleration);
    ps01SetFullStepSpeed_chain(config->fullstep_speed);

    ps01SetParam_chain(ST_SLP,     config->st_slp);
    ps01SetParam_chain(FN_SLP_ACC, config->fn_slp_acc);
    ps01SetParam_chain(FN_SLP_DEC, config->fn_slp_dec);

    ps01SetOcThreshold_chain(config->oc_threshold);
    ps01SetStallThreshold_chain(config->stall_threshold);
    ps01SetAlarms_chain(ALARM_STALL | ALARM_CMD_ERR | ALARM_OVC | ALARM_THRM_SD | ALARM_THRM_WARN | ALARM_UVLO);
    ps01GetStatus_chain();
}

int32_t RARM_GetPositionDegrees(uint8_t joint_index)
{
    if (joint_index >= N_JOINTS) return 0;
    mot_bank->active = joint_index;
    int32_t  steps        = ps01GetPosition_chain();
    int64_t  steps_per_rev = mot_bank->motors[joint_index].steps_rev;
    int64_t  ratio         = mot_bank->motors[joint_index].reduction_ratio;
    int64_t  microsteps    = mot_bank->motors[joint_index].stepmode.bits.STEP_SEL;
    return (int32_t)((int64_t)steps * 360 / (steps_per_rev << microsteps) / ratio);
}

int32_t RARM_GetPositionMilliDegrees(uint8_t joint_index)
{
    if (joint_index >= N_JOINTS) return 0;
    mot_bank->active = joint_index;
    int32_t  steps        = ps01GetPosition_chain();
    int64_t  steps_per_rev = mot_bank->motors[joint_index].steps_rev;
    int64_t  ratio         = mot_bank->motors[joint_index].reduction_ratio;
    int64_t  microsteps    = mot_bank->motors[joint_index].stepmode.bits.STEP_SEL;
    return (int32_t)((int64_t)steps * 360000LL / (steps_per_rev << microsteps) / ratio);
}

void RARM_GearboxMoveDegrees(RARM_Gearbox_t *gearbox, int16_t degs)
{
    RARM_MoveDegrees(gearbox->mot1_index, degs);
    RARM_MoveDegrees(gearbox->mot2_index, -degs);
}

void RARM_GearboxRotateDegrees(RARM_Gearbox_t *gearbox, int16_t degs)
{
    RARM_MoveDegrees(gearbox->mot1_index, degs);
    RARM_MoveDegrees(gearbox->mot2_index, degs);
}

void RARM_MoveDegrees(uint8_t joint_index, int16_t degs)
{
    RARM_MoveMilliDegrees(joint_index, (int32_t)degs * 1000);
}

void RARM_MoveMilliDegrees(uint8_t joint_index, int32_t mdeg)
{
    if (joint_index >= N_JOINTS) return;

    mot_bank->active = joint_index;

    int32_t current_mdeg = RARM_GetPositionMilliDegrees(joint_index);
    int32_t target_mdeg  = current_mdeg + mdeg;

    if (target_mdeg < mot_bank->motors[joint_index].min_degs * 1000)
        target_mdeg = mot_bank->motors[joint_index].min_degs * 1000;
    else if (target_mdeg > mot_bank->motors[joint_index].max_degs * 1000)
        target_mdeg = mot_bank->motors[joint_index].max_degs * 1000;

    int32_t delta = target_mdeg - current_mdeg;

    uint8_t dir = CLOCKWISE;
    if (delta < 0) {
        delta = -delta;
        dir = COUNTER_CLOCKWISE;
    }

    ps01MoveDegrees_chain(dir, (uint32_t)delta);
}

void RARM_Run(uint8_t joint_index, uint8_t dir, uint16_t rpm)
{
    if (joint_index >= N_JOINTS) return;
    mot_bank->active = joint_index;
    uint32_t steps_s = (uint32_t)rpm * mot_bank->motors[joint_index].steps_rev / 60;
    ps01Run_chain(dir, steps_s);
}

void RARM_HardBrake(uint8_t joint_index)
{
    if (joint_index >= N_JOINTS) return;
    mot_bank->active = joint_index;
    ps01HardStop_chain();
}

void RARM_SoftBrake(uint8_t joint_index)
{
    if (joint_index >= N_JOINTS) return;
    mot_bank->active = joint_index;
    ps01SoftStop_chain();
}

void RARM_HardHiZ(uint8_t joint_index)
{
    if (joint_index >= N_JOINTS) return;
    mot_bank->active = joint_index;
    ps01HardHiZ_chain();
}

void RARM_SoftHiZ(uint8_t joint_index)
{
    if (joint_index >= N_JOINTS) return;
    mot_bank->active = joint_index;
    ps01SoftHiZ_chain();
}

void RARM_ReleaseBrake(uint8_t joint_index)
{
    if (joint_index == J2_INDEX) {
        HAL_GPIO_WritePin(J2_BRAKE_GPIO_Port, J2_BRAKE_Pin, GPIO_PIN_SET);
    } else if (joint_index == J3_INDEX) {
        HAL_GPIO_WritePin(J3_BRAKE_GPIO_Port, J3_BRAKE_Pin, GPIO_PIN_SET);
    }
}

void RARM_EngageBrake(uint8_t joint_index)
{
    if (joint_index == J2_INDEX) {
        HAL_GPIO_WritePin(J2_BRAKE_GPIO_Port, J2_BRAKE_Pin, GPIO_PIN_RESET);
    } else if (joint_index == J3_INDEX) {
        HAL_GPIO_WritePin(J3_BRAKE_GPIO_Port, J3_BRAKE_Pin, GPIO_PIN_RESET);
    }
}

bool RARM_IsMoving(uint8_t joint_index)
{
    if (joint_index >= N_JOINTS) return false;
    mot_bank->active = joint_index;
    uint16_t status = ps01GetStatus_chain();
    return (status & STATUS_BUSY_BIT) == 0;
}
