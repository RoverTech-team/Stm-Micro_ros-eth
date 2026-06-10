/*
 * GPIO pin map — NUCLEO-H755ZI-Q (board MB1363) + rover arm carrier.
 *
 * The H755ZI-Q exposes three sets of headers:
 *   - Arduino-compatible Zio headers CN7 (right, top) and CN8 (left, top)
 *     silk-screened D0..D13, A0..A5, plus the 6-pin ICSP/SPI header.
 *   - Zio extension headers CN9 (right, bottom) and CN10 (left, bottom).
 *   - Morpho headers CN11 (right) and CN12 (left), each 2x38 pins,
 *     giving direct access to all STM32 signals that are not on Zio.
 *
 * On-board LEDs and push-button are not brought out to any header.
 * The DRV_CS signal remains on Morpho CN11. J2_BRAKE, J3_BRAKE and
 * DRV_RESET now exit on the Arduino Zio headers as D5, D6 and D7.
 *
 * Reference: ST UM2408 (NUCLEO-H755ZI-Q user manual) and MB1363 schematic.
 *
 *   Pin  | Header         | Dir / Mode         | Signal    | Notes
 *  ------+----------------+--------------------+-----------+------------------------------------------
 *   PB0  | on-board LED1  | out PP, low-speed  | LED_GREEN | shared with ST-LINK VCP, no header access
 *   PB14 | on-board LED2  | out PP, low-speed  | LED_RED   | no header access
 *  ------+----------------+--------------------+-----------+------------------------------------------
 *   PA5  | CN8 pin 4  (D13) | AF5  high-speed  | SPI1_SCK  | Arduino ICSP SCK; solder-bridge SB121 selects
 *   PA6  | CN8 pin 3  (D12) | AF5  high-speed  | SPI1_MISO | Arduino ICSP MISO
 *   PA7  | CN8 pin 2  (D11) | AF5  high-speed  | SPI1_MOSI | Arduino ICSP MOSI
 *  ------+----------------+--------------------+-----------+------------------------------------------
 *   PA4  | CN8 pin 1  (D5) | out PP, low-speed  | J2_BRAKE  | active HIGH: HIGH = released, LOW = engaged
 *   PB1  | CN10 pin 3 (D6) | out PP, low-speed  | J3_BRAKE  | active HIGH: HIGH = released, LOW = engaged
 *   PA8  | CN9 pin 8  (D7) | out PP, high-speed| DRV_RESET | powerSTEP01 reset, active LOW; held LOW 2.3 s
 *        |                  |                    |           | at boot to wait for the 24 V rail to settle
 *   PD14 | Morpho CN11      | out PP, high-speed| DRV_CS    | SPI chip-select for the daisy-chain
 *
 * Notes on H755-specific concerns:
 *   - PB14 is also TIM1_CH2 / BDMA channel — not used here, just be aware.
 *   - PG9 carries the LSE bypass on some H7 packages; not the case on the
 *     H755ZIT6 we solder, but worth checking if a sibling board misbehaves.
 *   - On the Nucleo-H755ZI-Q the SPI1_NSS line is PA4 (Arduino D10 / CN8-1);
 *     we keep NSS in software (`SPI_NSS_SOFT`) and do not configure PA4.
 *   - All brake / driver / reset lines go to the arm-controller carrier.
 *     The exact Morpho pin numbers (CN11 / CN12 row-column) are tracked in
 *     the carrier's wiring diagram; see `docs/wiring.md` (TODO).
 */

#include "gpio.h"

void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  GPIO_InitStruct.Pin   = LED_GREEN_Pin | LED_RED_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GREEN_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin | LED_RED_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = DRV_CS_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DRV_CS_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(DRV_CS_GPIO_Port, DRV_CS_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin   = DRV_RESET_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DRV_RESET_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(DRV_RESET_GPIO_Port, DRV_RESET_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = J2_BRAKE_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(J2_BRAKE_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(J2_BRAKE_GPIO_Port, J2_BRAKE_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = J3_BRAKE_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(J3_BRAKE_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(J3_BRAKE_GPIO_Port, J3_BRAKE_Pin, GPIO_PIN_RESET);
}
