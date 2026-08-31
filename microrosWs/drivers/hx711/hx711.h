/**
 * @file hx711.h
 * @brief STM32 HAL adaptation layer for HX711 Load Cell Amplifier
 */

#ifndef HX711_H
#define HX711_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief HX711 Context Structure
 */
typedef struct {
    GPIO_TypeDef *dout_port; /*!< Data Out GPIO Port */
    uint16_t dout_pin;       /*!< Data Out GPIO Pin */
    
    GPIO_TypeDef *pd_sck_port; /*!< Power Down and Serial Clock GPIO Port */
    uint16_t pd_sck_pin;       /*!< Power Down and Serial Clock GPIO Pin */
    
    uint8_t gain;            /*!< Current gain setting (128, 64, or 32) */
    long offset;             /*!< Offset (tare value) */
    float scale;             /*!< Scale factor */
} hx711_t;

/**
 * @brief Initialize the HX711 context.
 * 
 * @param hx         Pointer to HX711 context.
 * @param dout_port  GPIO Port for DOUT.
 * @param dout_pin   GPIO Pin for DOUT.
 * @param sck_port   GPIO Port for SCK.
 * @param sck_pin    GPIO Pin for SCK.
 */
void hx711_init(hx711_t *hx, GPIO_TypeDef *dout_port, uint16_t dout_pin, GPIO_TypeDef *sck_port, uint16_t sck_pin);

/**
 * @brief Check if HX711 is ready to be read.
 * 
 * @param hx Pointer to HX711 context.
 * @return true if ready, false otherwise.
 */
bool hx711_is_ready(hx711_t *hx);

/**
 * @brief Set the gain factor (128 or 64 for channel A, 32 for channel B).
 * 
 * @param hx   Pointer to HX711 context.
 * @param gain Gain value (128, 64, or 32).
 */
void hx711_set_gain(hx711_t *hx, uint8_t gain);

/**
 * @brief Read data from HX711.
 * 
 * @param hx Pointer to HX711 context.
 * @return 24-bit reading (sign-extended to 32 bits).
 */
long hx711_read(hx711_t *hx);

/**
 * @brief Read data and average it over a number of times.
 * 
 * @param hx    Pointer to HX711 context.
 * @param times Number of times to read and average.
 * @return Averaged 24-bit reading.
 */
long hx711_read_average(hx711_t *hx, uint8_t times);

/**
 * @brief Get the value without the offset (reading - offset).
 * 
 * @param hx    Pointer to HX711 context.
 * @param times Number of times to read and average.
 * @return Averaged reading minus offset.
 */
double hx711_get_value(hx711_t *hx, uint8_t times);

/**
 * @brief Get the value scaled by the scale factor ((reading - offset) / scale).
 * 
 * @param hx    Pointer to HX711 context.
 * @param times Number of times to read and average.
 * @return Scaled reading.
 */
float hx711_get_units(hx711_t *hx, uint8_t times);

/**
 * @brief Set the offset based on the current reading (tare).
 * 
 * @param hx    Pointer to HX711 context.
 * @param times Number of times to read and average.
 */
void hx711_tare(hx711_t *hx, uint8_t times);

/**
 * @brief Set the scale factor.
 * 
 * @param hx    Pointer to HX711 context.
 * @param scale New scale factor.
 */
void hx711_set_scale(hx711_t *hx, float scale);

/**
 * @brief Get the current scale factor.
 * 
 * @param hx Pointer to HX711 context.
 * @return Current scale factor.
 */
float hx711_get_scale(hx711_t *hx);

/**
 * @brief Set the offset manually.
 * 
 * @param hx     Pointer to HX711 context.
 * @param offset New offset.
 */
void hx711_set_offset(hx711_t *hx, long offset);

/**
 * @brief Get the current offset.
 * 
 * @param hx Pointer to HX711 context.
 * @return Current offset.
 */
long hx711_get_offset(hx711_t *hx);

/**
 * @brief Put HX711 in power down mode.
 * 
 * @param hx Pointer to HX711 context.
 */
void hx711_power_down(hx711_t *hx);

/**
 * @brief Wake up HX711 from power down mode.
 * 
 * @param hx Pointer to HX711 context.
 */
void hx711_power_up(hx711_t *hx);

#ifdef __cplusplus
}
#endif

#endif /* HX711_H */
