/**
 * @file hx711.c
 * @brief STM32 HAL adaptation layer for HX711 Load Cell Amplifier
 */

#include "hx711.h"

// Simple software delay for ~1 microsecond
// Note: This might need adjustment depending on the exact CPU clock frequency.
// It is intended to provide a brief pause for the SCK pin toggle.
static inline void hx711_delay_us(void) {
    // 1000 NOPs provides ~4 us pulse width on STM32H7 CM4 (240MHz)
    // This prevents signal ringing and extra clock pulses on long jumper wires.
    for (volatile uint32_t i = 0; i < 1000; i++) {
        __NOP();
    }
}

/**
 * @brief Shift in 8 bits of data.
 * @param hx Pointer to HX711 context.
 * @return 8-bit read data.
 */
static uint8_t hx711_shift_in(hx711_t *hx) {
    uint8_t value = 0;
    for (uint8_t i = 0; i < 8; ++i) {
        HAL_GPIO_WritePin(hx->pd_sck_port, hx->pd_sck_pin, GPIO_PIN_SET);
        hx711_delay_us();
        value |= (HAL_GPIO_ReadPin(hx->dout_port, hx->dout_pin) == GPIO_PIN_SET ? 1 : 0) << (7 - i);
        HAL_GPIO_WritePin(hx->pd_sck_port, hx->pd_sck_pin, GPIO_PIN_RESET);
        hx711_delay_us();
    }
    return value;
}

void hx711_init(hx711_t *hx, GPIO_TypeDef *dout_port, uint16_t dout_pin, GPIO_TypeDef *sck_port, uint16_t sck_pin) {
    hx->dout_port = dout_port;
    hx->dout_pin = dout_pin;
    hx->pd_sck_port = sck_port;
    hx->pd_sck_pin = sck_pin;
    hx->offset = 0;
    hx->scale = 1.0f;
    
    // Default gain to 128 (Channel A)
    hx711_set_gain(hx, 128);
}

bool hx711_is_ready(hx711_t *hx) {
    return (HAL_GPIO_ReadPin(hx->dout_port, hx->dout_pin) == GPIO_PIN_RESET);
}

void hx711_set_gain(hx711_t *hx, uint8_t gain) {
    switch (gain) {
        case 128:       // channel A, gain factor 128
            hx->gain = 1;
            break;
        case 64:        // channel A, gain factor 64
            hx->gain = 3;
            break;
        case 32:        // channel B, gain factor 32
            hx->gain = 2;
            break;
        default:
            // default to channel A, gain factor 128
            hx->gain = 1;
            break;
    }
}

long hx711_read(hx711_t *hx) {
    // Wait for the chip to become ready.
    // In a non-blocking system, it might be better to return an error,
    // but we wait to mimic Arduino library behavior.
    while (!hx711_is_ready(hx)) {
        HAL_Delay(0); // Yield or minimal delay
    }

    uint8_t data[3] = {0};
    uint8_t filler = 0x00;

    // Pulse the clock pin 24 times to read the data.
    // Disable interrupts to prevent timing issues during bit-banging.
    __disable_irq();

    data[2] = hx711_shift_in(hx);
    data[1] = hx711_shift_in(hx);
    data[0] = hx711_shift_in(hx);

    // Set the channel and the gain factor for the next reading using the clock pin.
    for (unsigned int i = 0; i < hx->gain; i++) {
        HAL_GPIO_WritePin(hx->pd_sck_port, hx->pd_sck_pin, GPIO_PIN_SET);
        hx711_delay_us();
        HAL_GPIO_WritePin(hx->pd_sck_port, hx->pd_sck_pin, GPIO_PIN_RESET);
        hx711_delay_us();
    }

    __enable_irq();

    // Replicate the most significant bit to pad out a 32-bit signed integer
    if (data[2] & 0x80) {
        filler = 0xFF;
    } else {
        filler = 0x00;
    }

    // Construct a 32-bit signed integer
    long value = ( (unsigned long)filler << 24
                 | (unsigned long)data[2] << 16
                 | (unsigned long)data[1] << 8
                 | (unsigned long)data[0] );

    return value;
}

long hx711_read_average(hx711_t *hx, uint8_t times) {
    long sum = 0;
    for (uint8_t i = 0; i < times; i++) {
        sum += hx711_read(hx);
        // Yield occasionally if times is large
        if (times > 1) {
            HAL_Delay(0); 
        }
    }
    return sum / times;
}

double hx711_get_value(hx711_t *hx, uint8_t times) {
    return hx711_read_average(hx, times) - hx->offset;
}

float hx711_get_units(hx711_t *hx, uint8_t times) {
    return (float)hx711_get_value(hx, times) / hx->scale;
}

void hx711_tare(hx711_t *hx, uint8_t times) {
    double sum = hx711_read_average(hx, times);
    hx711_set_offset(hx, (long)sum);
}

void hx711_set_scale(hx711_t *hx, float scale) {
    hx->scale = scale;
}

float hx711_get_scale(hx711_t *hx) {
    return hx->scale;
}

void hx711_set_offset(hx711_t *hx, long offset) {
    hx->offset = offset;
}

long hx711_get_offset(hx711_t *hx) {
    return hx->offset;
}

void hx711_power_down(hx711_t *hx) {
    HAL_GPIO_WritePin(hx->pd_sck_port, hx->pd_sck_pin, GPIO_PIN_RESET);
    hx711_delay_us();
    HAL_GPIO_WritePin(hx->pd_sck_port, hx->pd_sck_pin, GPIO_PIN_SET);
}

void hx711_power_up(hx711_t *hx) {
    HAL_GPIO_WritePin(hx->pd_sck_port, hx->pd_sck_pin, GPIO_PIN_RESET);
}
