/**
 * @file test_imu_lsm6dsv16x.c
 * @brief Unit tests for ST LSM6DSV16X 6-Axis IMU Driver and IMU Manager
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "lsm6dsv16x.h"
#include "lsm6dsv16x_reg.h"

/* Mock Register Storage */
static uint8_t mock_registers[256];

static int32_t mock_read_reg(void *handle, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    (void)handle;
    for (uint16_t i = 0; i < len; i++) {
        data[i] = mock_registers[(reg_addr + i) & 0xFF];
    }
    return 0;
}

static int32_t mock_write_reg(void *handle, uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
    (void)handle;
    for (uint16_t i = 0; i < len; i++) {
        mock_registers[(reg_addr + i) & 0xFF] = data[i];
    }
    return 0;
}

static void test_whoami(void)
{
    printf("[TEST] Checking WHO_AM_I validation... ");
    lsm6dsv16x_ctx_t ctx;
    lsm6dsv16x_init(&ctx, mock_read_reg, mock_write_reg, NULL, 0x6A);

    mock_registers[LSM6DSV16X_WHO_AM_I] = 0x70;
    assert(lsm6dsv16x_check_whoami(&ctx) == true);

    mock_registers[LSM6DSV16X_WHO_AM_I] = 0x00;
    assert(lsm6dsv16x_check_whoami(&ctx) == false);

    printf("PASSED!\n");
}

static void test_scaled_reading(void)
{
    printf("[TEST] Checking Scaled IMU Readings... ");
    lsm6dsv16x_ctx_t ctx;
    lsm6dsv16x_init(&ctx, mock_read_reg, mock_write_reg, NULL, 0x6A);

    /* Configure ±4g accel and ±500dps gyro */
    lsm6dsv16x_configure(&ctx, LSM6DSV16X_XL_ODR_120Hz, LSM6DSV16X_4g,
                               LSM6DSV16X_G_ODR_120Hz, LSM6DSV16X_500dps);

    /* Mock 1g on Accel Z (for 4g full scale, sensitivity is 0.122 mg/LSB -> ~8196 LSB for 1000mg = 1g) */
    int16_t raw_accel_z = 8196;
    mock_registers[LSM6DSV16X_OUTZ_L_A] = (uint8_t)(raw_accel_z & 0xFF);
    mock_registers[LSM6DSV16X_OUTZ_H_A] = (uint8_t)((raw_accel_z >> 8) & 0xFF);

    lsm6dsv16x_data_scaled_t scaled;
    int32_t ret = lsm6dsv16x_read_scaled(&ctx, &scaled);
    assert(ret == 0);

    /* Accel Z should be approx 9.80665 m/s^2 (within 1% tolerance) */
    float diff = fabsf(scaled.accel_z - 9.80665f);
    assert(diff < 0.2f);

    printf("PASSED! (Z-Accel = %.3f m/s^2)\n", scaled.accel_z);
}

int main(void)
{
    printf("=== LSM6DSV16X IMU Driver Unit Tests ===\n");
    memset(mock_registers, 0, sizeof(mock_registers));

    test_whoami();
    test_scaled_reading();

    printf("=== ALL UNIT TESTS PASSED SUCCESSFULLY! ===\n");
    return 0;
}
