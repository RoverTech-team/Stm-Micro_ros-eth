#ifndef RARM_H
#define RARM_H

#include <stdbool.h>
#include <stdint.h>
#include "../powerSTEP01/ps01.h"

#define N_JOINTS 6

#define J1_INDEX 0
#define J2_INDEX 1
#define J3_INDEX 2
#define J4_INDEX 3
#define J5_INDEX 4
#define J6_INDEX 5

#define CLOCKWISE         1
#define COUNTER_CLOCKWISE 0

/* NOTE (bench 2026-09-05): a J1<->J2 chain-swap was suspected from eyewitness
 * reports and a RARM_DrvIdx() remap was briefly added, but a brake-anchored
 * test disproved it (index-0 command with J2 brake engaged strained the
 * braked joint => chain position N-1-active already addresses the right
 * motor). Mapping is 1:1, NO remap. Do NOT re-add index translation without
 * repeating the brake-anchored test. */

typedef struct
{
    uint8_t   OVERCURRENT_SD;
    uint8_t   VSCOMP;
    uint8_t   STEP_MODE;
    uint32_t  steps_rev;
    uint16_t  reduction_ratio;
    float     supply_voltage;
    float     run_acc_dec_voltage;
    float     hold_voltage;

    uint16_t  max_speed;
    uint16_t  min_speed;
    uint16_t  acceleration;
    uint16_t  deceleration;
    uint16_t  fullstep_speed;

    float     oc_threshold;
    float     stall_threshold;
    int32_t   min_degs;
    int32_t   max_degs;

    uint8_t   st_slp;
    uint8_t   fn_slp_acc;
    uint8_t   fn_slp_dec;
    uint16_t  int_speed;
} RARM_SimpleConfig_t;

typedef struct
{
    uint8_t mot1_index;
    uint8_t mot2_index;
} RARM_Gearbox_t;

void    RARM_SetBank               (StepperBank_t *bank);
void    RARM_SetConfig             (uint8_t joint_index, RARM_SimpleConfig_t *config);

int32_t RARM_GetPositionDegrees    (uint8_t joint_index);
int32_t RARM_GetPositionMilliDegrees(uint8_t joint_index);

void    RARM_GearboxMoveDegrees    (RARM_Gearbox_t *gearbox, int16_t degs);
void    RARM_GearboxRotateDegrees  (RARM_Gearbox_t *gearbox, int16_t degs);
void    RARM_MoveDegrees           (uint8_t joint_index, int16_t degs);
void    RARM_MoveMilliDegrees      (uint8_t joint_index, int32_t mdeg);
void    RARM_Run                   (uint8_t joint_index, uint8_t dir, uint16_t rpm);
void    RARM_HardBrake             (uint8_t joint_index);
void    RARM_SoftBrake             (uint8_t joint_index);
void    RARM_HardHiZ               (uint8_t joint_index);
void    RARM_SoftHiZ               (uint8_t joint_index);

void    RARM_ReleaseBrake          (uint8_t joint_index);
void    RARM_EngageBrake           (uint8_t joint_index);

bool    RARM_IsMoving              (uint8_t joint_index);

extern int32_t j1_sim_mdeg;  // J1 simulated position for bench test (no physical motor at J1)

#endif
