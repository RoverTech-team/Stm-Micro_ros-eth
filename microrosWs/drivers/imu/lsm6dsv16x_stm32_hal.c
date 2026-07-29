/**
 * @file lsm6dsv16x_stm32_hal.c
 * @brief STM32 HAL adaptation layer implementation for LSM6DSV16X IMU (I2C and SPI)
 */

#include "lsm6dsv16x_stm32_hal.h"

/* --- I2C Callbacks --- */

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

    return lsm6dsv16x_init(ctx, lsm6dsv16x_stm32_i2c_read, lsm6dsv16x_stm32_i2c_write, ctx, addr_8bit);
}

#ifdef HAL_SPI_MODULE_ENABLED
/* --- SPI Callbacks --- */

int32_t lsm6dsv16x_stm32_spi_read(void *handle, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    if (handle == NULL || data == NULL) return -1;

    lsm6dsv16x_spi_handle_t *spi = (lsm6dsv16x_spi_handle_t *)handle;
    uint8_t addr_byte = reg_addr | 0x80U; /* SPI Read flag */

    /* Pull CS Low */
    if (spi->cs_port != NULL) {
        HAL_GPIO_WritePin(spi->cs_port, spi->cs_pin, GPIO_PIN_RESET);
    }

    /* Send Register Address */
    HAL_StatusTypeDef status = HAL_SPI_Transmit(spi->hspi, &addr_byte, 1, 100);

    /* Receive Data */
    if (status == HAL_OK) {
        status = HAL_SPI_Receive(spi->hspi, data, len, 100);
    }

    /* Release CS High */
    if (spi->cs_port != NULL) {
        HAL_GPIO_WritePin(spi->cs_port, spi->cs_pin, GPIO_PIN_SET);
    }

    return (status == HAL_OK) ? 0 : -1;
}

int32_t lsm6dsv16x_stm32_spi_write(void *handle, uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
    if (handle == NULL || data == NULL) return -1;

    lsm6dsv16x_spi_handle_t *spi = (lsm6dsv16x_spi_handle_t *)handle;
    uint8_t addr_byte = reg_addr & 0x7FU; /* SPI Write flag */

    /* Pull CS Low */
    if (spi->cs_port != NULL) {
        HAL_GPIO_WritePin(spi->cs_port, spi->cs_pin, GPIO_PIN_RESET);
    }

    /* Send Register Address */
    HAL_StatusTypeDef status = HAL_SPI_Transmit(spi->hspi, &addr_byte, 1, 100);

    /* Send Data */
    if (status == HAL_OK) {
        status = HAL_SPI_Transmit(spi->hspi, (uint8_t *)data, len, 100);
    }

    /* Release CS High */
    if (spi->cs_port != NULL) {
        HAL_GPIO_WritePin(spi->cs_port, spi->cs_pin, GPIO_PIN_SET);
    }

    return (status == HAL_OK) ? 0 : -1;
}

int32_t lsm6dsv16x_stm32_init_spi(lsm6dsv16x_ctx_t *ctx, lsm6dsv16x_spi_handle_t *spi_handle)
{
    if (ctx == NULL || spi_handle == NULL || spi_handle->hspi == NULL) return -1;

    /* Ensure CS pin starts HIGH */
    if (spi_handle->cs_port != NULL) {
        HAL_GPIO_WritePin(spi_handle->cs_port, spi_handle->cs_pin, GPIO_PIN_SET);
    }

    return lsm6dsv16x_init(ctx, lsm6dsv16x_stm32_spi_read, lsm6dsv16x_stm32_spi_write, (void *)spi_handle, 0);
}
#endif
