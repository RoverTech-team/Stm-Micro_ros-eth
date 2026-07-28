/**
 * @file lsm6dsv16x_stm32_hal.c
 * @brief STM32 HAL adaptation layer implementation for LSM6DSV16X IMU
 */

#include "lsm6dsv16x_stm32_hal.h"

int32_t lsm6dsv16x_stm32_i2c_read(void *handle, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    if (handle == NULL || data == NULL) return -1;
    
    lsm6dsv16x_ctx_t *ctx = (lsm6dsv16x_ctx_t *)handle;
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)ctx->handle;

    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(hi2c, ctx->i2c_addr, (uint16_t)reg_addr,
                                                I2C_MEMADD_SIZE_8BIT, data, len, 100);
    return (status == HAL_OK) ? 0 : -1;
}

int32_t lsm6dsv16x_stm32_i2c_write(void *handle, uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
    if (handle == NULL || data == NULL) return -1;

    lsm6dsv16x_ctx_t *ctx = (lsm6dsv16x_ctx_t *)handle;
    I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)ctx->handle;

    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(hi2c, ctx->i2c_addr, (uint16_t)reg_addr,
                                                 I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, len, 100);
    return (status == HAL_OK) ? 0 : -1;
}

int32_t lsm6dsv16x_stm32_init_i2c(lsm6dsv16x_ctx_t *ctx, I2C_HandleTypeDef *hi2c, uint8_t i2c_7bit_addr)
{
    if (ctx == NULL || hi2c == NULL) return -1;

    uint8_t addr_8bit = (i2c_7bit_addr != 0) ? (i2c_7bit_addr << 1) : LSM6DSV16X_I2C_ADD_L_8BIT;

    ctx->read_reg  = lsm6dsv16x_stm32_i2c_read;
    ctx->write_reg = lsm6dsv16x_stm32_i2c_write;
    ctx->handle    = (void *)hi2c;
    ctx->i2c_addr  = addr_8bit;

    /* Initialize default configuration (120Hz XL & Gyro) */
    return lsm6dsv16x_init(ctx, lsm6dsv16x_stm32_i2c_read, lsm6dsv16x_stm32_i2c_write, ctx, addr_8bit);
}
