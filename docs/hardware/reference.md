---
title: Hardware Reference
nav_order: 6
---

# Hardware Reference

## NUCLEO-H755ZI-Q

TODO: Add the official pinout diagram and any required board jumpers.

## JSN-SR04T Wiring

From the CM4 demo and simulation helper:

- TRIG: `PD1`
- ECHO: `PD0`
- Power: 5V sensor power (verify board wiring)
- Ground: common GND

TODO: Confirm the exact voltage-level strategy and any level shifting used.

## Flash Map

From linker scripts and Renode platform files:

- Flash bank 1 (CM7): `0x08000000`
- Flash bank 2 (CM4): `0x08100000`

Sources:
- `Test_Board_Sensore/Makefile/CM4/stm32h755xx_flash_CM4.ld`
- `Test_Board_Sensore/simulation/platform/nucleo_h755zi_q_dual.repl`
