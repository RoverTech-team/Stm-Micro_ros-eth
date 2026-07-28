/**
 * @file lsm6dsv16x.h
 * @brief Driver header for ST LSM6DSV16X 6-Axis IMU Sensor (MIKROE-5672 Smart DOF 2 Click)
 */

#ifndef LSM6DSV16X_H
#define LSM6DSV16X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "lsm6dsv16x_reg.h"

/* Standard Gravity Constant */
#ifndef STANDARD_GRAVITY
#define STANDARD_GRAVITY 9.80665f
#endif

/* Degrees to Radians Conversion */
#ifndef DEG_TO_RAD
#define DEG_TO_RAD (3.14159265358979323846f / 180.0f)
#endif

/* Read / Write Function Pointer Typedefs */
typedef int32_t (*lsm6dsv16x_write_ptr)(void *handle, uint8_t reg_addr, const uint8_t *data, uint16_t len);
typedef int32_t (*lsm6dsv16x_read_ptr)(void *handle, uint8_t reg_addr, uint8_t *data, uint16_t len);

/* Bus Interface Context */
typedef struct {
    lsm6dsv16x_write_ptr write_reg;
    lsm6dsv16x_read_ptr  read_reg;
    void                *handle;       /* Pointer to HAL bus instance e.g. &hi2c1 or &hspi1 */
    uint8_t              i2c_addr;     /* 8-bit I2C slave address (if applicable) */
    lsm6dsv16x_fs_xl_t   accel_fs;
    lsm6dsv16x_fs_g_t    gyro_fs;
    lsm6dsv16x_odr_xl_t  accel_odr;
    lsm6dsv16x_odr_g_t   gyro_odr;
} lsm6dsv16x_ctx_t;

/* Raw Sensor Data */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} lsm6dsv16x_axes_raw_t;

typedef struct {
    lsm6dsv16x_axes_raw_t accel;
    lsm6dsv16x_axes_raw_t gyro;
    int16_t temp_raw;
} lsm6dsv16x_data_raw_t;

/* Scaled Physical Data (SI Units: m/s^2 for Accel, rad/s for Gyro, °C for Temp) */
typedef struct {
    float accel_x;  /* m/s^2 */
    float accel_y;  /* m/s^2 */
    float accel_z;  /* m/s^2 */
    float gyro_x;   /* rad/s */
    float gyro_y;   /* rad/s */
    float gyro_z;   /* rad/s */
    float temp_c;   /* Celsius */
} lsm6dsv16x_data_scaled_t;

/* API Function Prototypes */

/**
 * @brief Initialize sensor context with bus callbacks
 */
int32_t lsm6dsv16x_init(lsm6dsv16x_ctx_t *ctx,
                        lsm6dsv16x_read_ptr read_fn,
                        lsm6dsv16x_write_ptr write_fn,
                        void *handle,
                        uint8_t i2c_addr);

/**
 * @brief Check WHO_AM_I register (returns true if 0x70)
 */
bool lsm6dsv16x_check_whoami(lsm6dsv16x_ctx_t *ctx);

/**
 * @brief Configure Accel & Gyro ODR, Full scale, and BDU
 */
int32_t lsm6dsv16x_configure(lsm6dsv16x_ctx_t *ctx,
                             lsm6dsv16x_odr_xl_t odr_xl, lsm6dsv16x_fs_xl_t fs_xl,
                             lsm6dsv16x_odr_g_t odr_g, lsm6dsv16x_fs_g_t fs_g);

/**
 * @brief Perform software reset of sensor
 */
int32_t lsm6dsv16x_reset(lsm6dsv16x_ctx_t *ctx);

/**
 * @brief Read raw accelerometer, gyroscope, and temperature readings
 */
int32_t lsm6dsv16x_read_raw(lsm6dsv16x_ctx_t *ctx, lsm6dsv16x_data_raw_t *raw);

/**
 * @brief Read scaled accelerometer, gyroscope, and temperature in SI units
 */
int32_t lsm6dsv16x_read_scaled(lsm6dsv16x_ctx_t *ctx, lsm6dsv16x_data_scaled_t *scaled);

/* Sensitivity Calculation Helpers */
float lsm6dsv16x_get_accel_sensitivity(lsm6dsv16x_fs_xl_t fs);
float lsm6dsv16x_get_gyro_sensitivity(lsm6dsv16x_fs_g_t fs);

#ifdef __cplusplus
}
#endif

#endif /* LSM6DSV16X_H */
