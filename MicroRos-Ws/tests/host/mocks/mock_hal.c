#include "mock_hal.h"
#include <string.h>

/* Mock GPIO instance */
GPIO_TypeDef GPIOB_Mock = {0};

/* Internal state */
static uint32_t mock_tick_count = 0;

void mock_hal_reset(void) {
    memset(&GPIOB_Mock, 0, sizeof(GPIOB_Mock));
    mock_tick_count = 0;
}

void mock_hal_set_tick(uint32_t tick) {
    mock_tick_count = tick;
}

void mock_hal_increment_tick(uint32_t delta) {
    mock_tick_count += delta;
}

void HAL_GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    if (GPIOx == &GPIOB_Mock) {
        GPIOx->State ^= (GPIO_Pin & 0xFF);
    }
}

void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t PinState) {
    if (GPIOx == &GPIOB_Mock) {
        if (PinState == GPIO_PIN_SET) {
            GPIOx->State |= (GPIO_Pin & 0xFF);
        } else {
            GPIOx->State &= ~(GPIO_Pin & 0xFF);
        }
    }
}

uint32_t HAL_GetTick(void) {
    return mock_tick_count;
}
