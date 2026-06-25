#include "standalone_path.h"
#include "shared_data.h"
#include "cmsis_os.h"
#include "stm32h7xx_hal.h"
#include <math.h>

#define JOINT_COUNT 6

extern __attribute__((section(".shared"))) shared_data_t shared_data_inst;
static volatile shared_data_t * const sensor_shared_data = &shared_data_inst;

void StartStandalonePathTask(void *argument)
{
  (void)argument;

  /* Shared SRAM4 is written by CM4, so invalidate D-cache before reading. */
  for (;;) {
    SCB_InvalidateDCache_by_Addr((void *)sensor_shared_data, (int32_t)sizeof(*sensor_shared_data));
    __DSB();
    if (sensor_shared_data->motor_ready) break;
    osDelay(10);
  }

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

  uint32_t const cyccnt_1ms = SystemCoreClock / 1000UL;
  uint32_t t0_ms = DWT->CYCCNT / cyccnt_1ms;
  uint32_t seq = 1;

  while (1) {
    uint32_t now_ms = DWT->CYCCNT / cyccnt_1ms;
    uint32_t elapsed = now_ms - t0_ms;

    if (elapsed >= STANDALONE_DURATION_MS) {
      for (int i = 0; i < JOINT_COUNT; i++) {
        sensor_shared_data->joint_cmd_positions[i] = 0;
      }
      sensor_shared_data->joint_cmd_seq = seq++;
      SCB_CleanDCache_by_Addr(
        (uint32_t *)&sensor_shared_data->joint_cmd_positions[0],
        sizeof(sensor_shared_data->joint_cmd_positions) +
        sizeof(sensor_shared_data->joint_cmd_seq));
      __DSB();
      osThreadExit();
    }

    float t_sec = (float)elapsed / 1000.0f;

    float j1 = 0.0f;
    float j2 = STANDALONE_AMP_DEG * sinf(2.0f * M_PI * 0.15f * t_sec + M_PI / 4.0f);
    float j3 = STANDALONE_AMP_DEG * sinf(2.0f * M_PI * 0.25f * t_sec + M_PI / 2.0f);
    float j6 = 0.0f;

    float cone_angle = 2.0f * M_PI * 0.10f * t_sec;
    float half_cone_rad = STANDALONE_AMP_DEG * M_PI / 180.0f;

    float dx = sinf(half_cone_rad) * cosf(cone_angle);
    float dy = sinf(half_cone_rad) * sinf(cone_angle);
    float dz = cosf(half_cone_rad);

    float j5_rad = -asinf(dy);
    float j4_rad = atan2f(dx, dz);

    float j4 = j4_rad * 180.0f / M_PI;
    float j5 = j5_rad * 180.0f / M_PI;

    /* Physical daisy-chain order (confirmed by sweep test):
     *   sw idx 0 = J1 (no motor)
     *   sw idx 1 = J5
     *   sw idx 2 = J6
     *   sw idx 3 = J4
     *   sw idx 4 = J3
     *   sw idx 5 = J2
     * Remap trajectory commands accordingly. */
    int32_t cmd_positions[JOINT_COUNT];
    cmd_positions[0] = (int32_t)lroundf(j1 * 1000.0f);
    cmd_positions[5] = (int32_t)lroundf(j2 * 1000.0f);
    cmd_positions[4] = (int32_t)lroundf(j3 * 1000.0f);
    cmd_positions[3] = (int32_t)lroundf(j4 * 1000.0f);
    cmd_positions[1] = (int32_t)lroundf(j5 * 1000.0f);
    cmd_positions[2] = (int32_t)lroundf(j6 * 1000.0f);

    for (int i = 0; i < JOINT_COUNT; i++) {
      sensor_shared_data->joint_cmd_positions[i] = cmd_positions[i];
    }
    sensor_shared_data->joint_cmd_seq = seq++;

    SCB_CleanDCache_by_Addr(
      (uint32_t *)&sensor_shared_data->joint_cmd_positions[0],
      sizeof(sensor_shared_data->joint_cmd_positions) +
      sizeof(sensor_shared_data->joint_cmd_seq));
    __DSB();

    osDelay(STANDALONE_UPDATE_MS);
  }
}
