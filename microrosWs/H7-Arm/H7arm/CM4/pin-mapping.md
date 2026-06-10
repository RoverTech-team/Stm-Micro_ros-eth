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
Morpho row/column numbers are tracked in the carrier wiring diagram
(`docs/wiring.md`, TODO).

---

## 1. Pin summary

| Signal       | Port.Pin | Header (board)     | Dir / Mode              | Speed   | Pull    | Notes                                              |
|--------------|----------|--------------------|-------------------------|---------|---------|----------------------------------------------------|
| LED_GREEN    | PB0      | on-board LED1      | Output PP               | Low     | None    | shared with ST-LINK VCP, no header access          |
| LED_RED      | PB14     | on-board LED2      | Output PP               | Low     | None    | no header access                                   |
| J2_BRAKE     | PA4      | CN8 pin 1 (D5)     | Output PP               | Low     | None    | active HIGH: HIGH = released, LOW = engaged        |
| J3_BRAKE     | PB1      | CN10 pin 3 (D6)    | Output PP               | Low     | None    | active HIGH: HIGH = released, LOW = engaged        |
| DRV_RESET    | PA8      | CN9 pin 8 (D7)     | Output PP               | High    | None    | powerSTEP01 reset, active LOW; held LOW 2.3 s at boot to wait for the 24 V rail |
| DRV_CS       | PD14     | Morpho CN11        | Output PP               | High    | None    | SPI1 chip-select for the daisy-chain (NSS in SW)   |
| SPI1_SCK     | PA5      | CN8 pin 4 (D13)    | AF5 (SPI1)              | High    | None    | Arduino ICSP SCK                                   |
| SPI1_MISO    | PA6      | CN8 pin 3 (D12)    | AF5 (SPI1)              | High    | None    | Arduino ICSP MISO                                  |
| SPI1_MOSI    | PA7      | CN8 pin 2 (D11)    | AF5 (SPI1)              | High    | None    | Arduino ICSP MOSI                                  |
| SPI1_NSS     | PA4      | CN8 pin 1 (D10)    | (not used, now J2_BRAKE)| —       | —       | kept in software (`SPI_NSS_SOFT`); PA4 repurposed  |

Pins not configured in `gpio.c` (deliberately):
- `PC13` B1 user button: not used on the arm controller. Pull the design
  forward only if a UI is required.
- `PA4` SPI1_NSS: NSS is handled in software by toggling `DRV_CS` on PD14.

---

## 2. GPIO configuration (from `Core/Src/gpio.c`)

`MX_GPIO_Init()` performs, in order:

1. Enables clocks: GPIOA, GPIOB, GPIOD, GPIOG.
2. `LED_GREEN_Pin | LED_RED_Pin` on GPIOB as output PP, low-speed,
   no pull, both driven LOW.
3. `DRV_CS_Pin` on GPIOD as output PP, high-speed, no pull, driven HIGH
   (deselected).
4. `DRV_RESET_Pin` on GPIOG as output PP, high-speed, no pull, driven LOW
   (held in reset during the 24 V rail stabilisation window).
5. `J2_BRAKE_Pin` on GPIOD as output PP, low-speed, no pull, driven LOW
   (engaged).
6. `J3_BRAKE_Pin` on GPIOA as output PP, low-speed, no pull, driven LOW
   (engaged).

Symbol definitions live in `Core/Inc/main.h`:

```c
#define LED_GREEN_Pin        GPIO_PIN_0
#define LED_GREEN_GPIO_Port  GPIOB
#define LED_RED_Pin          GPIO_PIN_14
#define LED_RED_GPIO_Port    GPIOB
#define DRV_RESET_Pin        GPIO_PIN_9
#define DRV_RESET_GPIO_Port  GPIOG
#define DRV_CS_Pin           GPIO_PIN_14
#define DRV_CS_GPIO_Port     GPIOD
#define J2_BRAKE_Pin         GPIO_PIN_15
#define J2_BRAKE_Pin         GPIO_PIN_15
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
| SPI1_MOSI  | PA7   | SPI1      | 5   |
| SPI1_NSS   | PD14  | GPIO (SW) | —   |

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
  24 V rail to settle (see `ps01_baremetal_os.c`). Deassert (drive HIGH)
  only after the rail is good.
- `DRV_CS`: idle HIGH, active LOW per SPI convention.
- On-board LEDs: sink-driven on this board — `LED_*_ON()` = GPIO_PIN_SET
  and `LED_*_OFF()` = GPIO_PIN_RESET.

---

## 6. H755-specific caveats

- PB14 is also TIM1_CH2 / BDMA channel; not used here, just be aware.
- PG9 carries the LSE bypass on some H7 packages; not the case on the
  H755ZIT6 we solder, but worth checking if a sibling board misbehaves.
- All brake / driver / reset lines go to the arm-controller carrier. The
  exact Morpho pin numbers (CN11 / CN12 row-column) are tracked in the
  carrier's wiring diagram; see `docs/wiring.md` (TODO).

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
| SPI1 SCK                | PA5          | CN8 pin 4 / **ICSP pin 3** (NOT D13)  | —                 |
| SPI1 MISO               | PA6          | CN8 pin 3 / **ICSP pin 1** (NOT D12)  | —                 |
| SPI1 MOSI               | PA7          | CN8 pin 2 / **ICSP pin 4** (NOT D11)  | —                 |
| J2_BRAKE                | PA4          | CN8 pin 1 / D5                        | was Morpho CN11   |
| J3_BRAKE                | PB1          | CN10 pin 3 / D6                       | was Morpho CN11   |
| DRV_RESET               | PA8          | CN9 pin 8 / D7                        | was Morpho CN12   |
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
| D0                  | PA3                  | PA3                     |
| D1                  | PA2                  | PA2                     |
| D2                  | PA10                 | PC3                     |
| D3                  | PB3                  | PA0                     |
| D4                  | PB5                  | PA1                     |
| D5                  | PB4                  | PA4                     |
| D6                  | PB10                 | PB1                     |
| D7                  | PA8                  | PA8                     |
| D8                  | PA9                  | PA9                     |
| D9                  | PC7                  | PA10                    |
| D10                 | PB6                  | PA11                    |
| D11                 | PA7                  | PB15                    |
| D12                 | PA6                  | PB14                    |
| D13                 | PA5                  | PB9                     |
| D14 (I2C1_SDA)      | PB9                  | PB9                     |
| D15 (I2C1_SCL)      | PB8                  | PB8                     |
| A0                  | PA0                  | PA0                     |
| A1                  | PA1                  | PA1                     |
| A2                  | PA4                  | PA4                     |
| A3                  | PB0                  | PB0                     |
| A4                  | PC1                  | PC1                     |
| A5                  | PC0                  | PC0                     |

Only **D0, D1, D7, D8, A0..A5** (10 of 22 silk-screened pins) carry the same
MCU pin on both boards. Every other D-pin swaps to a different GPIO, and
SPI1 in particular moves from D11/D12/D13 (F446 = PA7/PA6/PA5) to the ICSP
header on the H755 (still PA7/PA6/PA5, but the silk-screen "D11/D12/D13" on
the H755 are PB15/PB14/PB9 and are **not** wired to SPI1).

### 8.3 What this means for the arm-controller port

- A shield that wires `BK1` to the F446's D6 footprint (PB10) cannot be
  dropped onto the H755 and still hit `J2_BRAKE` — the H755 D6 is PB1, not
  PD15. `J2_BRAKE` (PD15) is on Morpho CN11 and must be wired to the
  carrier, not to a D6 shield header.
- A shield that wires to the F446's D10 (PB6) for `DRV_CS` will land on PA11
  on the H755 — that is GPIO_Output on the H7 .ioc and is *not* `DRV_CS`.
  `DRV_CS` is on PD14, Morpho CN11.
- The only application pins that line up with the F446 footprint without
  rewiring are: D7 (`BK2` / `J3_BRAKE`, both on PA8) and D8 (which is
  PA9 on both, but the F446 uses it for `DRV_RST` and the H755 does not).
- `SPI1` on PA5/PA6/PA7 lines up at the **ICSP** header on both boards; if
  the arm-carrier taps SPI off the Arduino ICSP header, the connection is
  portable. If it taps off D11/D12/D13, it is F446-only.
