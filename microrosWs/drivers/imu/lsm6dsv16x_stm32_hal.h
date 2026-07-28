/**
 * @file lsm6dsv16x_stm32_hal.h
 * @brief STM32 HAL adaptation layer for LSM6DSV16X IMU driver
 */

#ifndef LSM6DSV16X_STM32_HAL_H
#define LSM6DSV16X_STM32_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lsm6dsv16x.h"
#include "stm32h7xx_hal.h"

/**
 * @brief I2C Read callback wrapper for STM32 HAL
 */
int32_t lsm6dsv16x_stm32_i2c_read(void *handle, uint8_t reg_addr, uint8_t *data, uint16_t len);

/**
 * @brief I2C Write callback wrapper for STM32 HAL
 */
int32_t lsm6dsv16x_stm32_i2c_write(void *handle, uint8_t reg_addr, const uint8_t *data, uint16_t len);

/**
 * @brief Helper to initialize LSM6DSV16X over I2C on STM32 H7
 */
int32_t lsm6dsv16x_stm32_init_i2c(lsm6dsv16x_ctx_t *ctx, I2C_HandleTypeDef *hi2c, uint8_t i2c_7bit_addr);

#ifdef __cplusplus
}
#endif

#endif /* LSM6DSV16X_STM32_HAL_H */
