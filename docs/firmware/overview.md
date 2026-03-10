---
title: Overview
parent: Firmware
nav_order: 1
---

# Firmware Overview

The firmware targets STM32H755 dual-core devices and supports both single-core (CM4-only smoke) and CM7-first dual-core bring-up. Dual-core synchronization is driven by the HSEM peripheral and shared boot configuration.

## Hardware Target

- Primary target: NUCLEO-H755ZI-Q (dual-core STM32H755)
- CM7 flash bank: `0x08000000`
- CM4 flash bank: `0x08100000`

TODO: Confirm any other supported STM32H7 variants used in this repo.

## Dual-Core Boot Table

| Item | CM7 | CM4 |
| --- | --- | --- |
| Vector base | `0x08000000` | `0x08100000` |
| Boot sync | HSEM fast take/release | Waits for CM7 release |
| Mailbox | `0x10047F00` shared | `0x10047F00` shared |
| Boot flow | CM7 releases CM4, waits for ACK | CM4 ACKs back to CM7 |

Sources:
- `Test_Board_Sensore/CM7/Core/Src/main_smoke_dualcore_cm7.c`
- `Test_Board_Sensore/Makefile/CM4/stm32h755xx_flash_CM4.ld`
- `Test_Board_Sensore/simulation/platform/nucleo_h755zi_q_dual.repl`
