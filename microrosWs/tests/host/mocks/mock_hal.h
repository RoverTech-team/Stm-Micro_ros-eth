#ifndef MOCK_HAL_H
#define MOCK_HAL_H

#include <stdint.h>
#include <stdbool.h>

/* Mock GPIO types */
typedef struct {
    uint32_t Pin;
    uint8_t State;
} GPIO_TypeDef;

#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0
#define GPIO_PIN_0 (1U << 0)
#define GPIO_PIN_14 (1U << 14)

/* Mock GPIO instance */
extern GPIO_TypeDef GPIOB_Mock;

/* Mock HAL functions */
void HAL_GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint8_t PinState);
uint32_t HAL_GetTick(void);

/* Mock control functions for testing */
void mock_hal_reset(void);
void mock_hal_set_tick(uint32_t tick);
void mock_hal_increment_tick(uint32_t delta);

#endif /* MOCK_HAL_H */
