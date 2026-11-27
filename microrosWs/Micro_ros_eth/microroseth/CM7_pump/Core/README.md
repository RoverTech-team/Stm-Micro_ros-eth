# CM7_pump — Pump Controller (Core)

micro-ROS firmware for the NUCLEO-H755ZI-Q (STM32H755) that drives a pump
MOSFET gate with hardware PWM and accepts the setpoint over a ROS 2 topic.

Ported from `reference.c` (Arduino/RP2040). The control logic is identical
(0–100 % setpoint -> PWM duty cycle), but uses native STM32H7 HAL + micro-ROS.

## Pin mapping (MOSFET gate PWM)

| Signal            | Connector | Pin     | STM32 pin | Timer        |
|-------------------|-----------|---------|-----------|--------------|
| Pump PWM (gate)   | CN7       | 18 (D9) | PD15      | TIM4_CH4     |

Wire the MOSFET gate to **CN7 pin 18 (D9 / PD15)**.

### Why this pin
- `TIM1` is reserved as the HAL timebase (systick), so `TIM1_CHx`
  (PA8/PE11/PE13…) cannot be reused for PWM.
- `PB0` is `TIM3_CH3` (CN10 D33) but is already the **green status LED**
  in this firmware — not available.
- `PA6` (an earlier draft) is only `SPI1_MISO` (D12) on this board, not a PWM pin.
- `TIM4_CH4` on `PD15` is free (not used by LEDs, Ethernet, or USART3 debug).

### Alternative PWM pins (change the `#define`s in `Core/Inc/pump.h`)
| Connector | Pin      | STM32 pin | Timer        |
|-----------|----------|-----------|--------------|
| CN10      | D32      | PA0       | TIM2_CH1     |
| CN7       | D11      | PB5       | TIM3_CH2     |
| CN10      | D36      | PB10      | TIM2_CH3     |
| CN10      | D35      | PB11      | TIM2_CH4     |
| CN7       | D10      | PD14      | TIM4_CH3     |

Reference: `microrosWs/Docs/stm32h755ziq_pin_mapping.md`.

## Configuration (`Core/Inc/pump.h`)
- `PUMP_TIMER_INSTANCE` / `PUMP_PWM_CHANNEL` — timer + channel (default TIM2 / CH1).
- `PUMP_GPIO_PORT` / `PUMP_GPIO_PIN` / `PUMP_GPIO_AF` — output pin (default PA0 / AF1).
- `PUMP_PWM_FREQUENCY_HZ` — PWM frequency (default 10 kHz).
- `PUMP_PWM_PERIOD` — resolution (default 255, i.e. 8-bit like Arduino `analogWrite`).
- `PUMP_TOPIC_NAME` — ROS 2 topic (default `pump_setpoint`).

## Behavior
- At boot `Pump_Init()` configures the timer/GPIO and starts at 0 % duty.
- A dedicated FreeRTOS task (`StartPumpTask` in `Core/Src/main.c`) subscribes to
  `std_msgs/Int32` on `pump_setpoint` and calls `Pump_SetPercent()` with the value,
  clamped to the 0–100 % range.
- Setpoint is logged: `CM7: pump-set=<value>`.

## Build
```sh
make -C Makefile/CM7_pump all
```
Flash `Makefile/CM7_pump/build/MicroRosEth_CM7_pump.bin`, then publish
`std_msgs/Int32` to `pump_setpoint` (0–100).

### PWM hardware self-test (`main_test.c`)
Standalone variant (`Core/Src/main_test.c`) that needs no micro-ROS/network.
At boot it drives the PWM through: **50 % for 30 s → 0 % for 30 s → 100 % for 10 s**,
then repeats (green LED on during 50 %, red LED on during 100 %).
```sh
make -C Makefile/CM7_pump_test all
```
Flash `Makefile/CM7_pump_test/build/MicroRosEth_CM7_pump_test.bin` to verify the
MOSFET-gate wiring/scope on CN7-D9 (PD15). Flashing it replaces the micro-ROS
firmware until you rebuild/flash the `CM7_pump` target again.
