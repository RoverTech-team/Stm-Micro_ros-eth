/**
 * @file lsm6dsv16x_stm32_hal.h
 * @brief STM32 HAL adaptation layer for LSM6DSV16X IMU driver (supports both SPI and I2C)
 */

#ifndef LSM6DSV16X_STM32_HAL_H
#define LSM6DSV16X_STM32_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lsm6dsv16x.h"
#include "stm32h7xx_hal.h"

#ifdef HAL_SPI_MODULE_ENABLED
/* Structure to hold SPI hardware instance & CS pin info */
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef      *cs_port;
    uint16_t           cs_pin;
} lsm6dsv16x_spi_handle_t;
#endif

/**
 * @brief I2C Read callback wrapper for STM32 HAL
 */
int32_t lsm6dsv16x_stm32_i2c_read(void *handle, uint8_t reg_addr, uint8_t *data, uint16_t len);

/**
 * @brief I2C Write callback wrapper for STM32 HAL
 */
int32_t lsm6dsv16x_stm32_i2c_write(void *handle, uint8_t reg_addr, const uint8_t *data, uint16_t len);

#ifdef HAL_SPI_MODULE_ENABLED
/**
 * @brief SPI Read callback wrapper for STM32 HAL
 */
int32_t lsm6dsv16x_stm32_spi_read(void *handle, uint8_t reg_addr, uint8_t *data, uint16_t len);

/**
 * @brief SPI Write callback wrapper for STM32 HAL
 */
int32_t lsm6dsv16x_stm32_spi_write(void *handle, uint8_t reg_addr, const uint8_t *data, uint16_t len);
#endif

/**
 * @brief Helper to initialize LSM6DSV16X over I2C on STM32 H7
 */
int32_t lsm6dsv16x_stm32_init_i2c(lsm6dsv16x_ctx_t *ctx, I2C_HandleTypeDef *hi2c, uint8_t i2c_7bit_addr);

#ifdef HAL_SPI_MODULE_ENABLED
/**
 * @brief Helper to initialize LSM6DSV16X over SPI on STM32 H7
 */
int32_t lsm6dsv16x_stm32_init_spi(lsm6dsv16x_ctx_t *ctx, lsm6dsv16x_spi_handle_t *spi_handle);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LSM6DSV16X_STM32_HAL_H */
