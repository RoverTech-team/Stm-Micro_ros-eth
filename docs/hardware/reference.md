---
title: Hardware Reference
parent: Hardware
nav_order: 1
---

# Hardware Reference

## Supported Boards

| Board | MCU | Ethernet PHY | Notes |
|---|---|---|---|
| STM32H755I-EV | STM32H755XIH6 | LAN8742A | Primary target (dual-core) |
| STM32H743I-EVAL | STM32H743XIH6 | LAN8742A | Single-core variant |
| STM32H747I-DISCO | STM32H747XIH6 | LAN8742A | Dual-core discovery kit |
| NUCLEO-H743ZI2 | STM32H743ZIT6 | LAN8742A | Nucleo (single-core) |

## Network Wiring

| Connection | Detail |
|---|---|
| STM32 Ethernet | RJ45 to switch or direct to host |
| PHY | LAN8742A via RMII |
| STM32 IP | `192.168.50.2` (static) |
| Host/Agent IP | `192.168.50.1` |

## Debug Probe

| Probe | Support |
|---|---|
| ST-Link/V2 | ✅ Minimum |
| ST-Link/V3 | ✅ Recommended |

## Flash Commands

```bash
# OpenOCD
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build/MicroRosEth.elf verify reset exit"

# STM32CubeProgrammer CLI
STM32_Programmer_CLI -c port=SWD \
  -w build/MicroRosEth.elf 0x08000000 -v -rst
```
