#ifndef ENCODER_PINS_H
#define ENCODER_PINS_H

#include "stm32h7xx_hal.h"

/*
 * Encoder extension v2 — 18× general-purpose FT GPIOs for 5V encoders
 * Docs: docs/pin-usage-encoder-proposal.md §3 (v2 general-pool, NO Timer/PWM)
 * All 18 are FT/FT_a/FT_f (5V tolerant to 5.5V @VDD=3.3V), on Zio CN9/CN10/CN7
 * Board Function per Docs/stm32h755ziq_pin_mapping.md: I/O / QSPI / ADC / SAI / I2C
 * Morpho numbers from UM2408 §6.4
 */

// --- Group A: Zio CN10 I/O (3) ---
#define ENC1_A_Pin        GPIO_PIN_14  // PG14
#define ENC1_A_GPIO_Port  GPIOG        // CN10 D2  I/O           FT  — Morpho CN11 20
#define ENC1_B_Pin        GPIO_PIN_14  // PE14
#define ENC1_B_GPIO_Port  GPIOE        // CN10 D4  I/O           FT  — CN12 61
#define ENC1_Z_Pin        GPIO_PIN_6   // PE6
#define ENC1_Z_GPIO_Port  GPIOE        // CN10 D38 I/O           FT  — CN11 52

// --- Group B: Zio CN7 I/O + CN9 NC (3) ---
#define ENC2_A_Pin        GPIO_PIN_9   // PG9
#define ENC2_A_GPIO_Port  GPIOG        // CN7  D8  I/O           FT  — CN12 17
#define ENC2_B_Pin        GPIO_PIN_10  // PD10 — shares USB_PWR_EN, swap to PE3 if USB host needed
#define ENC2_B_GPIO_Port  GPIOD        // CN9  D64 I/O NC        FT  — CN12 57
#define ENC2_Z_Pin        GPIO_PIN_6   // PG6
#define ENC2_Z_GPIO_Port  GPIOG        // CN10 D26 QSPI_BK1_NCS  FT  — CN12 40

// --- Group C: QSPI_BK1 on CN10 (3) ---
#define ENC3_A_Pin        GPIO_PIN_13  // PD13
#define ENC3_A_GPIO_Port  GPIOD        // CN10 D28 QSPI_IO3      FT  — CN12 25
#define ENC3_B_Pin        GPIO_PIN_12  // PD12
#define ENC3_B_GPIO_Port  GPIOD        // CN10 D29 QSPI_IO1      FT  — CN12 27
#define ENC3_Z_Pin        GPIO_PIN_11  // PD11
#define ENC3_Z_GPIO_Port  GPIOD        // CN10 D30 QSPI_IO0      FT  — CN12 29

// --- Group D: ADC on CN10/CN9 (4) ---
#define ENC4_A_Pin        GPIO_PIN_6   // PF6  ADC3_INP8
#define ENC4_A_GPIO_Port  GPIOF        // CN10 A6  ADC           FT_a — CN12 32
#define ENC4_B_Pin        GPIO_PIN_10  // PF10 ADC3_INP6
#define ENC4_B_GPIO_Port  GPIOF        // CN10 A7  ADC           FT_a — CN12 36
#define ENC4_Z_Pin        GPIO_PIN_0   // PC0  ADC_123_INP10
#define ENC4_Z_GPIO_Port  GPIOC        // CN9  A1  ADC           FT_a — CN12 34
#define ENC5_A_Pin        GPIO_PIN_11  // PF11 ADC1_INP2
#define ENC5_A_GPIO_Port  GPIOF        // CN9  A5  ADC           FT   — CN11 55

// --- Group E: I2C2 + SAI on CN9 (5) ---
#define ENC5_B_Pin        GPIO_PIN_14  // PF14 I2C2_SCL
#define ENC5_B_GPIO_Port  GPIOF        // CN9  D69 I2C2_SCL      FT_f — CN12 62
#define ENC5_Z_Pin        GPIO_PIN_15  // PF15 I2C2_SDA
#define ENC5_Z_GPIO_Port  GPIOF        // CN9  D68 I2C2_SDA      FT_f — CN12 60
#define ENC6_A_Pin        GPIO_PIN_8   // PF8  SAI1_B_SCK
#define ENC6_A_GPIO_Port  GPIOF        // CN9  D61 SAI_B_SCK     FT   — CN12 64
#define ENC6_B_Pin        GPIO_PIN_7   // PF7  SAI1_B_MCLK
#define ENC6_B_GPIO_Port  GPIOF        // CN9  D62 SAI_B_MCLK    FT   — CN12 66
#define ENC6_Z_Pin        GPIO_PIN_9   // PF9  SAI1_B_FS
#define ENC6_Z_GPIO_Port  GPIOF        // CN9  D63 SAI_B_FS      FT   — CN12 68

// Count check: 18 pins
#define ENCODER_PIN_COUNT 18U

// FT verification (all FT/FT_a/FT_f, no TT):
//  H755 datasheet Table 7: PG14 FT, PE14 FT, PE6 FT, PG9 FT, PD10 FT, PG6 FT,
//  PD13 FT, PD12 FT, PD11 FT, PF6 FT_a, PF10 FT_a, PC0 FT_a, PF11 FT,
//  PF14 FT_f, PF15 FT_f, PF8 FT, PF7 FT, PF9 FT — all 5V tolerant input.

void MX_Encoder_GPIO_Init(void);
uint32_t Encoder_Test_SampleAll(void); // returns 18-bit sample

#endif
