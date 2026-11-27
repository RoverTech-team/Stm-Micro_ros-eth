#include "ps01calc.h"
#include <math.h>

#define STEPS_S_TO_MAXMIN  0.065536f
#define STEPS_S2_TO_ACC    0.06871948f
#define STEPS_S_TO_SPEED   67.108864f
#define KVAL_FULL_SCALE    256.0f

uint8_t calculateKVAL(float supply, float target)
{
    if (supply <= 0.0f) {
        return 0U;
    }
    float kval = (target / supply) * KVAL_FULL_SCALE;
    if (kval >= 255.0f) {
        return 255U;
    }
    if (kval <= 0.0f) {
        return 0U;
    }
    return (uint8_t)lroundf(kval);
}

uint16_t calculateMaxMinSpeed(uint16_t steps_s)
{
    if (steps_s > 15610U) {
        return 1023U;
    }
    return (uint16_t)lroundf((float)steps_s * STEPS_S_TO_MAXMIN);
}

uint16_t calculateAcceleration(uint16_t steps_s2)
{
    uint16_t acc_val = (uint16_t)lroundf((float)steps_s2 * STEPS_S2_TO_ACC);
    if (acc_val >= 0xFFFU) {
        return 0xFFEU;
    }
    return acc_val;
}

uint32_t calculateSpeed(uint16_t steps_s)
{
    if (steps_s > 15625U) {
        return 1048576U;
    }
    return (uint32_t)lroundf((float)steps_s * STEPS_S_TO_SPEED);
}

int32_t getStepsFromAngle(int32_t deg, Stepper_t *motor)
{
    int32_t abs_deg = (deg < 0) ? -deg : deg;
    int32_t revs    = abs_deg / 360;
    int32_t rem_deg = abs_deg - (revs * 360);

    int64_t full_rev_steps = (int64_t)motor->steps_rev * (int64_t)revs;
    int64_t rem_steps      = ((int64_t)motor->steps_rev * (int64_t)rem_deg) / 360;
    int64_t total_usteps   = (full_rev_steps + rem_steps)
                           << motor->stepmode.bits.STEP_SEL;
    int64_t total          = total_usteps * (int64_t)motor->reduction_ratio;

    return (deg < 0) ? -(int32_t)total : (int32_t)total;
}
