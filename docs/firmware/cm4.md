---
title: CM4
parent: Firmware
nav_order: 3
---

# CM4 Sensor Driver (JSN-SR04T)

The CM4 demo firmware implements a 4-phase bring-up sequence that validates GPIO wiring, echo readback, and pulse measurement timing for the JSN-SR04T ultrasonic sensor.

## 4-Phase Sequence

1. Phase 1: toggle TRIG (PD1) at 500 ms intervals for 5 seconds.
2. Phase 2: read raw ECHO (PD0) for 5 seconds.
3. Phase 3: trigger + echo attempts (10 tries) with timeout checks.
4. Phase 4: blink distance in cm on the green LED.

## Timing Constants

- `TRIG_RESET_TIME_US = 2`
- `TRIG_PULSE_TIME_US = 10`
- `ECHO_WAIT_TIMEOUT_US = 30000`
- `ECHO_PULSE_TIMEOUT_US = 26000`

These values are defined in `Test_Board_Sensore/CM4/Core/Src/main.c` and should be kept in sync with the simulation model.

## GPIO Map

- TRIG: `PD1`
- ECHO: `PD0`
- Green LED: `PB0`
- Red LED: `PB14`

TODO: Confirm any board-level remaps or alternate pinouts for hardware variants.

Sources:
- `Test_Board_Sensore/CM4/Core/Src/main.c`
- `Test_Board_Sensore/simulation/python/sensor_helper.py`
