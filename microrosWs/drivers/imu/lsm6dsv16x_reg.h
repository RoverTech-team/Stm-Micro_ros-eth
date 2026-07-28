/**
 * @file lsm6dsv16x_reg.h
 * @brief Register map definitions for ST LSM6DSV16X 6-Axis IMU Sensor (MIKROE-5672 Smart DOF 2 Click)
 */

#ifndef LSM6DSV16X_REG_H
#define LSM6DSV16X_REG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* I2C Address (7-bit address shifted or unshifted) */
#define LSM6DSV16X_I2C_ADD_L           0x6AU  /* SA0 = GND */
#define LSM6DSV16X_I2C_ADD_H           0x6BU  /* SA0 = VDD */
#define LSM6DSV16X_I2C_ADD_L_8BIT      (LSM6DSV16X_I2C_ADD_L << 1)
#define LSM6DSV16X_I2C_ADD_H_8BIT      (LSM6DSV16X_I2C_ADD_H << 1)

/* Register Map */
#define LSM6DSV16X_FUNC_CFG_ACCESS     0x01U
#define LSM6DSV16X_PIN_CTRL            0x02U
#define LSM6DSV16X_FIFO_CTRL1          0x07U
#define LSM6DSV16X_FIFO_CTRL2          0x08U
#define LSM6DSV16X_FIFO_CTRL3          0x09U
#define LSM6DSV16X_FIFO_CTRL4          0x0AU
#define LSM6DSV16X_COUNTER_BDR_REG1    0x0BU
#define LSM6DSV16X_COUNTER_BDR_REG2    0x0CU
#define LSM6DSV16X_INT1_CTRL           0x0DU
#define LSM6DSV16X_INT2_CTRL           0x0EU
#define LSM6DSV16X_WHO_AM_I            0x0FU  /* Value: 0x70 */

#define LSM6DSV16X_CTRL1_XL            0x10U  /* Accel ODR & Full scale */
#define LSM6DSV16X_CTRL2_G             0x11U  /* Gyro ODR & Full scale */
#define LSM6DSV16X_CTRL3_C             0x12U  /* Control reg 3: BDU, SW_RESET */
#define LSM6DSV16X_CTRL4_C             0x13U
#define LSM6DSV16X_CTRL5_C             0x14U
#define LSM6DSV16X_CTRL6_C             0x15U
#define LSM6DSV16X_CTRL7_C             0x16U
#define LSM6DSV16X_CTRL8_C             0x17U
#define LSM6DSV16X_CTRL9_C             0x18U
#define LSM6DSV16X_CTRL10_C            0x19U

#define LSM6DSV16X_FIFO_STATUS1        0x1BU
#define LSM6DSV16X_FIFO_STATUS2        0x1CU
#define LSM6DSV16X_ALL_INT_SRC         0x1DU
#define LSM6DSV16X_STATUS_REG          0x1EU  /* Data ready status */

#define LSM6DSV16X_OUT_TEMP_L          0x20U
#define LSM6DSV16X_OUT_TEMP_H          0x21U
#define LSM6DSV16X_OUTX_L_G            0x22U  /* Gyro X low byte */
#define LSM6DSV16X_OUTX_H_G            0x23U
#define LSM6DSV16X_OUTY_L_G            0x24U
#define LSM6DSV16X_OUTY_H_G            0x25U
#define LSM6DSV16X_OUTZ_L_G            0x26U
#define LSM6DSV16X_OUTZ_H_G            0x27U
#define LSM6DSV16X_OUTX_L_A            0x28U  /* Accel X low byte */
#define LSM6DSV16X_OUTX_H_A            0x29U
#define LSM6DSV16X_OUTY_L_A            0x2AU
#define LSM6DSV16X_OUTY_H_A            0x2BU
#define LSM6DSV16X_OUTZ_L_A            0x2CU
#define LSM6DSV16X_OUTZ_H_A            0x2DU

/* Sensor ID */
#define LSM6DSV16X_WHO_AM_I_VAL        0x70U

/* Bit Field Mask Definitions */

/* STATUS_REG bits */
#define LSM6DSV16X_STATUS_XLDA_MASK    0x01U  /* Accelerometer data available */
#define LSM6DSV16X_STATUS_GDA_MASK     0x02U  /* Gyroscope data available */
#define LSM6DSV16X_STATUS_TDA_MASK     0x04U  /* Temperature data available */

/* CTRL3_C bits */
#define LSM6DSV16X_CTRL3_SW_RESET_MASK 0x01U
#define LSM6DSV16X_CTRL3_BDU_MASK      0x40U  /* Block Data Update */
#define LSM6DSV16X_CTRL3_BOOT_MASK     0x80U

/* Accelerometer Full Scale Enum & Sensitivity (mg/LSB) */
typedef enum {
    LSM6DSV16X_2g  = 0x00,
    LSM6DSV16X_4g  = 0x01,
    LSM6DSV16X_8g  = 0x02,
    LSM6DSV16X_16g = 0x03
} lsm6dsv16x_fs_xl_t;

/* Accelerometer Output Data Rate (ODR) Enum */
typedef enum {
    LSM6DSV16X_XL_OFF      = 0x00,
    LSM6DSV16X_XL_ODR_1Hz5 = 0x01,
    LSM6DSV16X_XL_ODR_7Hz5 = 0x02,
    LSM6DSV16X_XL_ODR_15Hz = 0x03,
    LSM6DSV16X_XL_ODR_30Hz = 0x04,
    LSM6DSV16X_XL_ODR_60Hz = 0x05,
    LSM6DSV16X_XL_ODR_120Hz= 0x06,
    LSM6DSV16X_XL_ODR_240Hz= 0x07,
    LSM6DSV16X_XL_ODR_480Hz= 0x08,
    LSM6DSV16X_XL_ODR_960Hz= 0x09
} lsm6dsv16x_odr_xl_t;

/* Gyroscope Full Scale Enum (dps) */
typedef enum {
    LSM6DSV16X_125dps  = 0x00,
    LSM6DSV16X_250dps  = 0x01,
    LSM6DSV16X_500dps  = 0x02,
    LSM6DSV16X_1000dps = 0x03,
    LSM6DSV16X_2000dps = 0x04,
    LSM6DSV16X_4000dps = 0x05
} lsm6dsv16x_fs_g_t;

/* Gyroscope Output Data Rate (ODR) Enum */
typedef enum {
    LSM6DSV16X_G_OFF       = 0x00,
    LSM6DSV16X_G_ODR_7Hz5  = 0x02,
    LSM6DSV16X_G_ODR_15Hz  = 0x03,
    LSM6DSV16X_G_ODR_30Hz  = 0x04,
    LSM6DSV16X_G_ODR_60Hz  = 0x05,
    LSM6DSV16X_G_ODR_120Hz = 0x06,
    LSM6DSV16X_G_ODR_240Hz = 0x07,
    LSM6DSV16X_G_ODR_480Hz = 0x08,
    LSM6DSV16X_G_ODR_960Hz = 0x09
} lsm6dsv16x_odr_g_t;

#ifdef __cplusplus
}
#endif

#endif /* LSM6DSV16X_REG_H */
