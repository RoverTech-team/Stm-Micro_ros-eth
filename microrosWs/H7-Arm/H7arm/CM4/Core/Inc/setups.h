#ifndef __SETUPS_H__
#define __SETUPS_H__
#include "../Src/drivers/powerSTEP01/ps01.h"
#include "../Src/drivers/powerSTEP01/cfg_structs.h"
#include "../Src/drivers/RArm/rarm.h"
#include "main.h"

/* NOTE: values below are verbatim from reference Archivio setups.h.
 * J4/J5 reduction_ratio 4.8 truncates to 4: RARM_SimpleConfig_t.reduction_ratio
 * is uint16_t (rarm.h). Effective J4/J5 ratio is therefore 4 (20% short vs 4.8:1 HW).
 *
 * Back-EMF slopes (ST_SLP / FN_SLP_ACC / FN_SLP_DEC / INT_SPEED) MUST be assigned
 * in `conf`, not via ps01SetParam_chain(): RARM_SetConfig() rewrites those four
 * registers from `conf` at the end of its sequence (rarm.c), so chain calls made
 * here were silently clobbered by uninitialized stack values. Fixed 2026-11-04:
 * the four slope values are now assigned to `conf` (one block per joint).
 */

/* 24 V motor rail. Was hardcoded as 24 in earlier revisions. */
#ifndef SUPPLY_V
#define SUPPLY_V 24.0f
#endif

static inline void runSetups()
{
  RARM_SimpleConfig_t conf;

  /////////////////////////////////
  ///////////////////////////////// J1
  /////////////////////////////////
  conf.steps_rev = 200;
  conf.reduction_ratio = 125;
  conf.min_degs = -9000;
  conf.max_degs = 9000;

  // steps/s or steps/s²
  conf.acceleration = 1200;
  conf.deceleration = 1200;
  conf.fullstep_speed = 20000;
  conf.max_speed = 1000;
  conf.min_speed = 0;

  // volts
  conf.hold_voltage = 1.2;
  conf.run_acc_dec_voltage = 1;
  conf.supply_voltage = SUPPLY_V;

  // amps
  conf.oc_threshold = 3.6;
  conf.stall_threshold = 20;

  conf.OVERCURRENT_SD = OC_NOSHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;

  // back-EMF compensation slopes
  conf.st_slp = 0x48;       // starting slope ~0.038%/step/s
  conf.int_speed = 0x1241;
  conf.fn_slp_acc = 0x48;   // acceleration slope
  conf.fn_slp_dec = 0x48;   // deceleration slope

  mot_bank->active = 0;

  RARM_SetConfig(0, &conf);
  HAL_Delay(10);

  /////////////////////////////////
  ///////////////////////////////// J2
  /////////////////////////////////
  conf.steps_rev = 200;
  conf.reduction_ratio = 50;
  conf.min_degs = -9000;
  conf.max_degs = 9000;

  // steps/s or steps/s²
  conf.acceleration = 1200;
  conf.deceleration = 1200;
  conf.fullstep_speed = 20000;
  conf.max_speed = 600;
  conf.min_speed = 0;

  // volts
  conf.hold_voltage = 2.2;
  conf.run_acc_dec_voltage = 1.8;
  conf.supply_voltage = SUPPLY_V;

  // amps
  conf.oc_threshold = 4.6;
  conf.stall_threshold = 20;

  conf.OVERCURRENT_SD = OC_NOSHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;

  // back-EMF compensation slopes
  conf.st_slp = 0x28;       // starting slope ~0.038%/step/s
  conf.int_speed = 0xD69;
  conf.fn_slp_acc = 0x38;   // acceleration slope
  conf.fn_slp_dec = 0x38;   // deceleration slope

  mot_bank->active = 1;

  RARM_SetConfig(1, &conf);
  HAL_Delay(10);

  /////////////////////////////////
  ///////////////////////////////// J3
  /////////////////////////////////
  conf.steps_rev = 200;
  conf.reduction_ratio = 50;
  conf.min_degs = -120;
  conf.max_degs = 120;

  // steps/s or steps/s²
  conf.acceleration = 800;
  conf.deceleration = 800;
  conf.fullstep_speed = 20000;
  conf.max_speed = 600;
  conf.min_speed = 0;

  // volts
  conf.hold_voltage = 2.5;
  conf.run_acc_dec_voltage = 3.5;
  conf.supply_voltage = SUPPLY_V;

  // amps
  conf.oc_threshold = 3.6;
  conf.stall_threshold = 20;

  conf.OVERCURRENT_SD = OC_NOSHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;

  // back-EMF compensation slopes
  conf.st_slp = 0x1B;       // starting slope ~0.038%/step/s
  conf.int_speed = 0x1241;
  conf.fn_slp_acc = 0x48;   // acceleration slope
  conf.fn_slp_dec = 0x48;   // deceleration slope

  mot_bank->active = 2;

  RARM_SetConfig(2, &conf);
  HAL_Delay(10);

  /////////////////////////////////
  ///////////////////////////////// J4
  /////////////////////////////////
  conf.steps_rev = 200;
  conf.reduction_ratio = 4.8;
  conf.min_degs = -90;
  conf.max_degs = 90;

  // steps/s or steps/s²
  conf.acceleration = 300;
  conf.deceleration = 300;
  conf.fullstep_speed = 20000;
  conf.max_speed = 600;
  conf.min_speed = 0;

  // volts
  conf.hold_voltage = 2.6;
  conf.run_acc_dec_voltage = 2.34;
  conf.supply_voltage = SUPPLY_V;

  // amps
  conf.oc_threshold = 3.6;
  conf.stall_threshold = 20;

  conf.OVERCURRENT_SD = OC_SHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;

  // back-EMF compensation slopes
  conf.st_slp = 0x18;       // starting slope ~0.038%/step/s
  conf.int_speed = 0xD9;
  conf.fn_slp_acc = 0x38;   // acceleration slope
  conf.fn_slp_dec = 0x38;   // deceleration slope

  mot_bank->active = 3;

  RARM_SetConfig(3, &conf);
  HAL_Delay(10);

  /////////////////////////////////
  ///////////////////////////////// J5
  /////////////////////////////////
  conf.steps_rev = 200;
  conf.reduction_ratio = 4.8;
  conf.min_degs = -90;
  conf.max_degs = 90;

  // steps/s or steps/s²
  conf.acceleration = 300;
  conf.deceleration = 300;
  conf.fullstep_speed = 20000;
  conf.max_speed = 600;
  conf.min_speed = 0;

  // volts
  conf.hold_voltage = 2.6;
  conf.run_acc_dec_voltage = 2.34;
  conf.supply_voltage = SUPPLY_V;

  // amps
  conf.oc_threshold = 3.6;
  conf.stall_threshold = 20;

  conf.OVERCURRENT_SD = OC_SHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;

  // back-EMF compensation slopes
  conf.st_slp = 0x18;       // starting slope ~0.038%/step/s
  conf.int_speed = 0xD9;
  conf.fn_slp_acc = 0x38;   // acceleration slope
  conf.fn_slp_dec = 0x38;   // deceleration slope

  mot_bank->active = 4;

  RARM_SetConfig(4, &conf);
  HAL_Delay(10);

  /////////////////////////////////
  ///////////////////////////////// J6
  /////////////////////////////////
  conf.steps_rev = 200;
  conf.reduction_ratio = 30;
  conf.min_degs = -9000;
  conf.max_degs = 9000;

  // steps/s or steps/s²
  conf.acceleration = 6200;
  conf.deceleration = 6200;
  conf.fullstep_speed = 20000;
  conf.max_speed = 1400;
  conf.min_speed = 0;

  // volts
  conf.hold_voltage = 1.8;
  conf.run_acc_dec_voltage = 2.5;
  conf.supply_voltage = SUPPLY_V;

  // amps
  conf.oc_threshold = 3.6;
  conf.stall_threshold = 20;

  conf.OVERCURRENT_SD = OC_NOSHUTDOWN;
  conf.VSCOMP = VSCOMP_DISABLE;
  conf.STEP_MODE = SM_16_MICROSTEP;

  // back-EMF compensation slopes — no separate reference value for J6,
  // same as J5 for now (must be set: RARM_SetConfig writes all four).
  conf.st_slp = 0x18;
  conf.int_speed = 0xD9;
  conf.fn_slp_acc = 0x38;
  conf.fn_slp_dec = 0x38;

  mot_bank->active = 5;

  RARM_SetConfig(5, &conf);
  HAL_Delay(10);
}
#endif
