/**
 * @file lsm6dsv16x.c
 * @brief Driver implementation for ST LSM6DSV16X 6-Axis IMU Sensor (MIKROE-5672 Smart DOF 2 Click)
 */

#include "lsm6dsv16x.h"
#include <string.h>

int32_t lsm6dsv16x_init(lsm6dsv16x_ctx_t *ctx,
                        lsm6dsv16x_read_ptr read_fn,
                        lsm6dsv16x_write_ptr write_fn,
                        void *handle,
                        uint8_t i2c_addr)
{
    if (ctx == NULL || read_fn == NULL || write_fn == NULL) {
        return -1;
    }

    ctx->read_reg  = read_fn;
    ctx->write_reg = write_fn;
    ctx->handle    = handle;
    ctx->i2c_addr  = (i2c_addr != 0) ? i2c_addr : LSM6DSV16X_I2C_ADD_L_8BIT;
    ctx->accel_fs  = LSM6DSV16X_4g;
    ctx->gyro_fs   = LSM6DSV16X_500dps;
    ctx->accel_odr = LSM6DSV16X_XL_ODR_120Hz;
    ctx->gyro_odr  = LSM6DSV16X_G_ODR_120Hz;

    return 0;
}

bool lsm6dsv16x_check_whoami(lsm6dsv16x_ctx_t *ctx)
{
    if (ctx == NULL || ctx->read_reg == NULL) {
        return false;
    }

    uint8_t whoami_val = 0;
    int32_t ret = ctx->read_reg(ctx->handle, LSM6DSV16X_WHO_AM_I, &whoami_val, 1);
    
    return (ret == 0 && whoami_val == LSM6DSV16X_WHO_AM_I_VAL);
}

int32_t lsm6dsv16x_reset(lsm6dsv16x_ctx_t *ctx)
{
    if (ctx == NULL || ctx->write_reg == NULL) {
        return -1;
    }

    uint8_t ctrl3_c = LSM6DSV16X_CTRL3_SW_RESET_MASK;
    int32_t ret = ctx->write_reg(ctx->handle, LSM6DSV16X_CTRL3_C, &ctrl3_c, 1);
    if (ret != 0) return ret;

    /* Poll SW_RESET bit until cleared */
    uint8_t poll_val = LSM6DSV16X_CTRL3_SW_RESET_MASK;
    uint16_t timeout = 100;
    while ((poll_val & LSM6DSV16X_CTRL3_SW_RESET_MASK) && (timeout > 0)) {
        ctx->read_reg(ctx->handle, LSM6DSV16X_CTRL3_C, &poll_val, 1);
        timeout--;
    }

    return (timeout > 0) ? 0 : -2;
}

int32_t lsm6dsv16x_configure(lsm6dsv16x_ctx_t *ctx,
                             lsm6dsv16x_odr_xl_t odr_xl, lsm6dsv16x_fs_xl_t fs_xl,
                             lsm6dsv16x_odr_g_t odr_g, lsm6dsv16x_fs_g_t fs_g)
{
    if (ctx == NULL || ctx->write_reg == NULL) {
        return -1;
    }

    ctx->accel_fs  = fs_xl;
    ctx->gyro_fs   = fs_g;
    ctx->accel_odr = odr_xl;
    ctx->gyro_odr  = odr_g;

    /* Enable BDU (Block Data Update) to prevent reading split high/low bytes */
    uint8_t ctrl3_c = 0;
    ctx->read_reg(ctx->handle, LSM6DSV16X_CTRL3_C, &ctrl3_c, 1);
    ctrl3_c |= LSM6DSV16X_CTRL3_BDU_MASK;
    ctx->write_reg(ctx->handle, LSM6DSV16X_CTRL3_C, &ctrl3_c, 1);

    /* Configure Accelerometer (CTRL1_XL: ODR[7:4], FS[3:2]) */
    uint8_t ctrl1_xl = ((uint8_t)odr_xl << 4) | (((uint8_t)fs_xl & 0x03) << 2);
    int32_t ret = ctx->write_reg(ctx->handle, LSM6DSV16X_CTRL1_XL, &ctrl1_xl, 1);
    if (ret != 0) return ret;

    /* Configure Gyroscope (CTRL2_G: ODR[7:4], FS[3:0]) */
    uint8_t ctrl2_g = ((uint8_t)odr_g << 4) | ((uint8_t)fs_g & 0x0F);
    ret = ctx->write_reg(ctx->handle, LSM6DSV16X_CTRL2_G, &ctrl2_g, 1);

    return ret;
}

int32_t lsm6dsv16x_read_raw(lsm6dsv16x_ctx_t *ctx, lsm6dsv16x_data_raw_t *raw)
{
    if (ctx == NULL || ctx->read_reg == NULL || raw == NULL) {
        return -1;
    }

    uint8_t buffer[14]; /* Temp (2), Gyro (6), Accel (6) */
    int32_t ret = ctx->read_reg(ctx->handle, LSM6DSV16X_OUT_TEMP_L, buffer, 14);
    if (ret != 0) {
        return ret;
    }

    raw->temp_raw = (int16_t)(((uint16_t)buffer[1] << 8) | buffer[0]);

    raw->gyro.x   = (int16_t)(((uint16_t)buffer[3] << 8) | buffer[2]);
    raw->gyro.y   = (int16_t)(((uint16_t)buffer[5] << 8) | buffer[4]);
    raw->gyro.z   = (int16_t)(((uint16_t)buffer[7] << 8) | buffer[6]);

    raw->accel.x  = (int16_t)(((uint16_t)buffer[9] << 8) | buffer[8]);
    raw->accel.y  = (int16_t)(((uint16_t)buffer[11] << 8) | buffer[10]);
    raw->accel.z  = (int16_t)(((uint16_t)buffer[13] << 8) | buffer[12]);

    return 0;
}

float lsm6dsv16x_get_accel_sensitivity(lsm6dsv16x_fs_xl_t fs)
{
    switch (fs) {
        case LSM6DSV16X_2g:  return 0.061f; /* mg/LSB */
        case LSM6DSV16X_4g:  return 0.122f;
        case LSM6DSV16X_8g:  return 0.244f;
        case LSM6DSV16X_16g: return 0.488f;
        default:             return 0.122f;
    }
}

float lsm6dsv16x_get_gyro_sensitivity(lsm6dsv16x_fs_g_t fs)
{
    switch (fs) {
        case LSM6DSV16X_125dps:  return 4.375f;  /* mdps/LSB */
        case LSM6DSV16X_250dps:  return 8.750f;
        case LSM6DSV16X_500dps:  return 17.500f;
        case LSM6DSV16X_1000dps: return 35.000f;
        case LSM6DSV16X_2000dps: return 70.000f;
        case LSM6DSV16X_4000dps: return 140.000f;
        default:                 return 17.500f;
    }
}

int32_t lsm6dsv16x_read_scaled(lsm6dsv16x_ctx_t *ctx, lsm6dsv16x_data_scaled_t *scaled)
{
    if (ctx == NULL || scaled == NULL) {
        return -1;
    }

    lsm6dsv16x_data_raw_t raw;
    int32_t ret = lsm6dsv16x_read_raw(ctx, &raw);
    if (ret != 0) {
        return ret;
    }

    float xl_sens_mg = lsm6dsv16x_get_accel_sensitivity(ctx->accel_fs);
    float g_sens_mdps = lsm6dsv16x_get_gyro_sensitivity(ctx->gyro_fs);

    /* Convert Accel (mg -> m/s^2) */
    float xl_factor = (xl_sens_mg / 1000.0f) * STANDARD_GRAVITY;
    scaled->accel_x = (float)raw.accel.x * xl_factor;
    scaled->accel_y = (float)raw.accel.y * xl_factor;
    scaled->accel_z = (float)raw.accel.z * xl_factor;

    /* Convert Gyro (mdps -> rad/s) */
    float g_factor = (g_sens_mdps / 1000.0f) * DEG_TO_RAD;
    scaled->gyro_x  = (float)raw.gyro.x * g_factor;
    scaled->gyro_y  = (float)raw.gyro.y * g_factor;
    scaled->gyro_z  = (float)raw.gyro.z * g_factor;

    /* Temperature (°C) */
    scaled->temp_c  = ((float)raw.temp_raw / 256.0f) + 25.0f;

    return 0;
}
