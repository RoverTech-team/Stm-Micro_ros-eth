#ifndef __SETUPS_H__
#define __SETUPS_H__
#include "../Src/drivers/powerSTEP01/ps01.h"
#include "../Src/drivers/powerSTEP01/cfg_structs.h"
#include "../Src/drivers/RArm/rarm.h"
#include "main.h"

/* Motor table for the wiretest firmware (main_wiretest.c -> runSetups()).
 * Values follow the reference Archivio setups.h, updated 2026-11-04:
 *   J1 hold 1.2V / run 1.0V, J2 hold 2.2V, J3 +-120deg & max 600,
 *   J4/J5 accel 300 & hold 2.6V, J6 ratio 30 / accel 6200 / max 1400.
 *
 * Slopes are set through `conf` only: RARM_SetConfig() writes ST_SLP /
 * FN_SLP_ACC / FN_SLP_DEC / INT_SPEED from `conf` at the end of its sequence,
 * so ps01SetParam_chain() calls made here would be redundant (and silently
 * overridden). Keep them in `conf`.
 *
 * J4/J5 reduction_ratio is 4 due to a uint16_t field (rarm.h): 4.8:1 HW
 * truncates to 4 (20% short).
 */

static inline void runSetups(void)
{
  RARM_SimpleConfig_t conf;

  // J1 — 125:1
  conf.steps_rev = 200;
  conf.reduction_ratio = 125;
  conf.min_degs = -9000;
  conf.max_degs = 9000;
  conf.acceleration = 1200;
  conf.deceleration = 1200;
  conf.fullstep_speed = 20000;
  conf.max_speed = 1000;
  conf.min_speed = 0;
  conf.hold_voltage = 1.2f;
  conf.run_acc_dec_voltage = 1.0f;
  conf.supply_voltage = SUPPLY_V;
  conf.oc_threshold = 3.6f;
  conf.stall_threshold = 20.0f;
  conf.OVERCURRENT_SD = OC_NOSHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;
  conf.st_slp = 0x48;
  conf.fn_slp_acc = 0x48;
  conf.fn_slp_dec = 0x48;
  conf.int_speed = 0x1241;
  mot_bank->active = 0;
  RARM_SetConfig(0, &conf);
  HAL_Delay(10);

  // J2 — 50:1
  conf.steps_rev = 200;
  conf.reduction_ratio = 50;
  conf.min_degs = -9000;
  conf.max_degs = 9000;
  conf.acceleration = 1200;
  conf.deceleration = 1200;
  conf.fullstep_speed = 20000;
  conf.max_speed = 600;
  conf.min_speed = 0;
  conf.hold_voltage = 2.2f;
  conf.run_acc_dec_voltage = 1.8f;
  conf.supply_voltage = SUPPLY_V;
  conf.oc_threshold = 4.6f;
  conf.stall_threshold = 20.0f;
  conf.OVERCURRENT_SD = OC_NOSHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;
  conf.st_slp = 0x28;
  conf.fn_slp_acc = 0x38;
  conf.fn_slp_dec = 0x38;
  conf.int_speed = 0x0D69;
  mot_bank->active = 1;
  RARM_SetConfig(1, &conf);
  HAL_Delay(10);

  // J3 — 50:1
  conf.steps_rev = 200;
  conf.reduction_ratio = 50;
  conf.min_degs = -120;
  conf.max_degs = 120;
  conf.acceleration = 800;
  conf.deceleration = 800;
  conf.fullstep_speed = 20000;
  conf.max_speed = 600;
  conf.min_speed = 0;
  conf.hold_voltage = 2.5f;
  conf.run_acc_dec_voltage = 3.5f;
  conf.supply_voltage = SUPPLY_V;
  conf.oc_threshold = 3.6f;
  conf.stall_threshold = 20.0f;
  conf.OVERCURRENT_SD = OC_NOSHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;
  conf.st_slp = 0x1B;
  conf.fn_slp_acc = 0x48;
  conf.fn_slp_dec = 0x48;
  conf.int_speed = 0x1241;
  mot_bank->active = 2;
  RARM_SetConfig(2, &conf);
  HAL_Delay(10);

  // J4 — 4.8:1 (truncated to 4, see note above)
  conf.steps_rev = 200;
  conf.reduction_ratio = 4;
  conf.min_degs = -90;
  conf.max_degs = 90;
  conf.acceleration = 300;
  conf.deceleration = 300;
  conf.fullstep_speed = 20000;
  conf.max_speed = 600;
  conf.min_speed = 0;
  conf.hold_voltage = 2.6f;
  conf.run_acc_dec_voltage = 2.34f;
  conf.supply_voltage = SUPPLY_V;
  conf.oc_threshold = 3.6f;
  conf.stall_threshold = 20.0f;
  conf.OVERCURRENT_SD = OC_SHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;
  conf.st_slp = 0x18;
  conf.fn_slp_acc = 0x38;
  conf.fn_slp_dec = 0x38;
  conf.int_speed = 0x0D9;
  mot_bank->active = 3;
  RARM_SetConfig(3, &conf);
  HAL_Delay(10);

  // J5 — 4.8:1 (truncated to 4, see note above)
  conf.steps_rev = 200;
  conf.reduction_ratio = 4;
  conf.min_degs = -90;
  conf.max_degs = 90;
  conf.acceleration = 300;
  conf.deceleration = 300;
  conf.fullstep_speed = 20000;
  conf.max_speed = 600;
  conf.min_speed = 0;
  conf.hold_voltage = 2.6f;
  conf.run_acc_dec_voltage = 2.34f;
  conf.supply_voltage = SUPPLY_V;
  conf.oc_threshold = 3.6f;
  conf.stall_threshold = 20.0f;
  conf.OVERCURRENT_SD = OC_SHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;
  conf.st_slp = 0x18;
  conf.fn_slp_acc = 0x38;
  conf.fn_slp_dec = 0x38;
  conf.int_speed = 0x0D9;
  mot_bank->active = 4;
  RARM_SetConfig(4, &conf);
  HAL_Delay(10);

  // J6 — 30:1 (no separate reference value for J6 BEMF: same as J4/J5)
  conf.steps_rev = 200;
  conf.reduction_ratio = 30;
  conf.min_degs = -9000;
  conf.max_degs = 9000;
  conf.acceleration = 6200;
  conf.deceleration = 6200;
  conf.fullstep_speed = 20000;
  conf.max_speed = 1400;
  conf.min_speed = 0;
  conf.hold_voltage = 1.8f;
  conf.run_acc_dec_voltage = 2.5f;
  conf.supply_voltage = SUPPLY_V;
  conf.oc_threshold = 3.6f;
  conf.stall_threshold = 20.0f;
  conf.OVERCURRENT_SD = OC_NOSHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;
  conf.st_slp = 0x18;
  conf.fn_slp_acc = 0x38;
  conf.fn_slp_dec = 0x38;
  conf.int_speed = 0x0D9;
  mot_bank->active = 5;
  RARM_SetConfig(5, &conf);
  HAL_Delay(10);
}
#endif
