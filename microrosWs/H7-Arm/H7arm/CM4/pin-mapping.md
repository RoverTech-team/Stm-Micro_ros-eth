# CM4 GPIO Pin Mapping — NUCLEO-H755ZI-Q (MB1363) + rover-arm carrier

Target: STM32H755ZIT6, CM4 core, board NUCLEO-H755ZI-Q (ST MB1363), driving the
rover-arm carrier PCB.

Header reference (ST UM2408):
- Arduino Zio headers CN7 (right, top) and CN8 (left, top): D0..D13, A0..A5
  plus the 6-pin ICSP/SPI header.
- Zio extension headers CN9 (right, bottom) and CN10 (left, bottom).
- Morpho headers CN11 (right) and CN12 (left), 2x38 pins each, giving direct
  access to all STM32 signals that are not on Zio.

On-board LEDs (PB0, PB14) and the user button (PC13) are not brought out to any
header. The DRV_CS / DRV_RESET / J*_BRAKE signals exit the Nucleo on the
Morpho headers and continue on the arm-controller carrier PCB; the exact
Morpho row/column numbers are tracked in the carrier wiring diagram.

---

## 1. Pin summary

| Signal       | Port.Pin | Header (board)     | Dir / Mode              | Speed   | Pull    | Notes                                              |
|--------------|----------|--------------------|-------------------------|---------|---------|----------------------------------------------------|
| LED_GREEN    | PB0      | on-board LED1      | Output PP               | Low     | None    | shared with ST-LINK VCP, no header access          |
| LED_RED      | PB14     | on-board LED2      | Output PP               | Low     | None    | no header access                                   |
| J2_BRAKE     | PE11     | CN10 D5            | Output PP               | Low     | None    | active HIGH: HIGH = released, LOW = engaged        |
| J3_BRAKE     | PA8      | CN10 D6            | Output PP               | Low     | None    | active HIGH: HIGH = released, LOW = engaged        |
| DRV_RESET    | PG12     | CN10 D7            | Output PP               | High    | None    | powerSTEP01 reset, active LOW; held LOW 2.3 s at boot to wait for the 24 V rail |
| DRV_CS       | PD14     | CN7 D10            | Output PP               | High    | None    | SPI1 chip-select for the daisy-chain (NSS in SW)   |
| SPI1_SCK     | PA5      | ICSP pin 3         | AF5 (SPI1)              | High    | None    | Arduino ICSP SCK                                   |
| SPI1_MISO    | PA6      | ICSP pin 1         | AF5 (SPI1)              | High    | None    | Arduino ICSP MISO                                  |
| SPI1_MOSI    | PB5      | CN7 pin 14 / D11   | AF5 (SPI1)              | High    | None    | Arduino header SPI_A_MOSI / shared TIM3_CH2        |

Pins deliberately not configured:
- `PA4` — hardware SPI1_NSS pin, but NSS is handled in software (`SPI_NSS_SOFT`); left unconfigured to avoid conflict.
- `PC13` B1 user button: not used on the arm controller.

---

## 2. GPIO configuration (from `Core/Src/gpio.c`)

`MX_GPIO_Init()` performs, in order:

1. Enables clocks: GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOG.
2. `LED_GREEN_Pin | LED_RED_Pin` on GPIOB as output PP, low-speed,
   no pull, both driven LOW.
3. `DRV_CS_Pin` on GPIOD (PD14) as output PP, high-speed, no pull, driven HIGH
   (deselected).
4. `DRV_RESET_Pin` on GPIOG (PG12) as output PP, high-speed, no pull, driven LOW
   (held in reset during the 24 V rail stabilisation window).
5. `J2_BRAKE_Pin` on GPIOE (PE11) as output PP, low-speed, no pull, driven LOW
   (engaged).
6. `J3_BRAKE_Pin` on GPIOA (PA8) as output PP, low-speed, no pull, driven LOW
   (engaged).

Symbol definitions live in `Core/Inc/main.h`:

```c
#define LED_GREEN_Pin        GPIO_PIN_0
#define LED_GREEN_GPIO_Port  GPIOB
#define LED_RED_Pin          GPIO_PIN_14
#define LED_RED_GPIO_Port    GPIOB
#define DRV_RESET_Pin        GPIO_PIN_12
#define DRV_RESET_GPIO_Port  GPIOG
#define DRV_CS_Pin           GPIO_PIN_14
#define DRV_CS_GPIO_Port     GPIOD
#define J2_BRAKE_Pin         GPIO_PIN_11
#define J2_BRAKE_GPIO_Port   GPIOE
#define J3_BRAKE_Pin         GPIO_PIN_8
#define J3_BRAKE_GPIO_Port   GPIOA
```

Convenience macros for the LEDs:

```c
#define LED_GREEN_ON()   HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET)
#define LED_GREEN_OFF()  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET)
#define LED_RED_ON()     HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   GPIO_PIN_SET)
#define LED_RED_OFF()    HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   GPIO_PIN_RESET)
```

---

## 3. SPI1 bus (from `Core/Src/spi.c`)

| Function   | Pin   | Alternate | AF# |
|------------|-------|-----------|-----|
| SPI1_SCK   | PA5   | SPI1      | 5   |
| SPI1_MISO  | PA6   | SPI1      | 5   |
| SPI1_MOSI  | PB5   | SPI1      | 5   |
| SPI1_NSS   | PA4   | GPIO (SW) | —   |

SPI1 configuration:
- Master, full-duplex, 8-bit, MSB first.
- Clock polarity HIGH, phase 2EDGE (CPOL=1, CPHA=1) — powerSTEP01 mode 0/3
  compatible.
- `NSS = SPI_NSS_SOFT`, `NSSPMode = SPI_NSS_PULSE_DISABLE`,
  `MasterKeepIOState = ENABLE` so the bus state survives an SPI abort.
- Baud prescaler `/32` from the APB clock; revisit if the loop back from the
  daisy-chain is too slow.

---

## 4. Timer usage (from `Core/Src/tim.c`)

- TIM2 is a free-running microsecond counter (PSC = 223 ⇒ 1 MHz tick,
  ARR = 0xFFFFFFFF). Used by `Timer2_NowUs()`, `DelayUs()`, `DelayMs()`.
  No GPIO is required for TIM2 in counter-only mode; if TIM2 channels are
  ever used for a step pulse, the corresponding pins (PA0/PA1/PA2/PA3 on
  TIM2_CH1..CH4) must be added to `gpio.c` and the `gpio.h` macros.

---

## 5. Active-level / electrical notes

- `J2_BRAKE`, `J3_BRAKE`: active HIGH, default LOW (engaged). The firmware
  must drive them HIGH before commanding motion, and return them to LOW on
  any fault or shutdown.
- `DRV_RESET`: active LOW, held LOW at boot for at least 2.3 s to allow the
  24 V rail to settle. Deassert (drive HIGH) only after the rail is good.
- `DRV_CS`: idle HIGH, active LOW per SPI convention.
- On-board LEDs: sink-driven on this board — `LED_*_ON()` = GPIO_PIN_SET
  and `LED_*_OFF()` = GPIO_PIN_RESET.

---

## 6. H755-specific caveats

- PB14 is also TIM1_CH2 / BDMA channel; not used here, just be aware.
- PG9 carries the LSE bypass on some H7 packages; not the case on the
  H755ZIT6 we solder, but worth checking if a sibling board misbehaves.
- PA4 is the hardware SPI1_NSS pin; we keep NSS in software and leave PA4
  unconfigured to avoid accidental conflicts.

---

## 7. Cross-reference: F446 → H755 naming

The `nucleo_f446` reference uses different labels for the same physical
roles. The CM4 port keeps the original role name plus the original signal
name on the H755 so the schematic and the firmware stay aligned.

| Role                  | F446 label | H755 label  | H755 pin |
|-----------------------|------------|-------------|----------|
| Driver chip-select    | DRV_CS     | DRV_CS      | PD14     |
| Driver reset          | DRV_RST    | DRV_RESET   | PG9      |
| Brake on joint 2      | BK1        | J2_BRAKE    | PD15     |
| Brake on joint 3      | BK2        | J3_BRAKE    | PA8      |
| User button           | B1         | (unused)    | —        |
| Step pulse (AF)       | PB5 TIM3_CH2 | (not on H7 CM4 yet) | — |

---

## 8. Header pin comparison: F446 (NUCLEO-F446) vs H755 (NUCLEO-H755ZI-Q)

Both boards expose the Arduino Uno R3-compatible header (D0..D15, A0..A5,
ICSP) on CN7/CN8, plus Morpho headers for the rest. **The silk-screen labels
(D0..D15) are the same, but the underlying MCU pin is not.** This is the
single biggest source of "it works on the F4 but not the H7" porting bugs.

### 8.1 Application pins: where each role appears on the board

| Role                    | H755 MCU pin | H755 header position                  | Prior Morpho path |
|-------------------------|--------------|---------------------------------------|-------------------|
| USART2 TX (console)     | PA2          | CN8 pin 1 / D1 (USART_TX)             | —                 |
| USART2 RX (console)     | PA3          | CN8 pin 2 / D0 (USART_RX)             | —                 |
| SPI1 SCK                | PA5          | ICSP pin 3                            | —                 |
| SPI1 MISO               | PA6          | ICSP pin 1                            | —                 |
| SPI1 MOSI               | PA7          | ICSP pin 4                            | —                 |
| J2_BRAKE                | PD15         | Morpho CN11                           | Morpho CN11       |
| J3_BRAKE                | PA8          | Morpho CN11                           | Morpho CN11       |
| DRV_RESET               | PG9          | Morpho CN12                           | Morpho CN12       |
| DRV_CS                  | PD14         | Morpho CN11                           | unchanged         |
| User button (B1)        | (unused)     | on-board B1 available but not wired   | n/a               |
| Status LED              | PB0 (green), PB14 (red) | on-board LED1, LED2 (no header) | n/a (different LEDs) |
| Step pulse (TIM AF)     | (not on H7 CM4 yet) | —                              | —                 |

Sources: F446 from `microrosWs/nucleo_f446/nucleo_f446.ioc` and ST UM1819
(NUCLEO-F446ZE) / UM1724 (NUCLEO-F446RE); H755 from ST UM2408
(NUCLEO-H755ZI-Q) and the comment block in `Core/Src/gpio.c`.

### 8.2 Side-by-side Arduino silk-screen → MCU pin

For reference when porting shields between the two Nucleo boards:

| Arduino silk-screen | F446 (NUCLEO-F446RE) | H755 (NUCLEO-H755ZI-Q) |
|---------------------|----------------------|-------------------------|
| D0                  | PA3                  | PB7                     |
| D1                  | PA2                  | PB6                     |
| D2                  | PA10                 | PG14                    |
| D3                  | PB3                  | PE13                    |
| D4                  | PB5                  | PE14                    |
| D5                  | PB4                  | PE11                    |
| D6                  | PB10                 | PA8                     |
| D7                  | PA8                  | PG12                    |
| D8                  | PA9                  | PG9                     |
| D9                  | PC7                  | PD15                    |
| D10                 | PB6                  | PD14                    |
| D11                 | PA7                  | PB5                     |
| D12                 | PA6                  | PA6                     |
| D13                 | PA5                  | PA5                     |
| D14 (I2C1_SDA)      | PB9                  | PB9                     |
| D15 (I2C1_SCL)      | PB8                  | PB8                     |
| A0                  | PA0                  | PA3                     |
| A1                  | PA1                  | PC0                     |
| A2                  | PA4                  | PC3                     |
| A3                  | PB0                  | PB1                     |
| A4                  | PC1                  | PC2 or PB9              |
| A5                  | PC0                  | PF11 or PB8             |

Only **D14, D15** carry the same MCU pin on both boards. Every other D-pin
and every A-pin swaps to a different GPIO. SPI1 on the H755 stays on
**PA5/PA6/PB5** (D13/D12/D11), same as the F446 but MOSI moved from
PA7 (F446) to PB5 (H755).

### 8.3 What this means for the arm-controller port

The arm-controller carrier uses the **Arduino D-header pins** directly
(not the ICSP header), so every signal maps through the D-pin cross-reference
table above.

The actual carrier wiring (confirmed on hardware):

| Signal      | Arduino pin | H755 MCU pin |
|-------------|-------------|--------------|
| DRV_CS      | D10         | PD14         |
| SPI1_MOSI   | D11         | PB5          |
| SPI1_MISO   | D12         | PA6          |
| SPI1_SCK    | D13         | PA5          |
| J2_BRAKE    | D5          | PE11         |
| J3_BRAKE    | D6          | PA8          |
| DRV_RESET   | D7          | PG12         |
