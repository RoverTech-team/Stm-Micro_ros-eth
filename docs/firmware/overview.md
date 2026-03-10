---
title: Firmware Overview
parent: Firmware
nav_order: 1
---

# Firmware Overview

The firmware runs on the **STM32H7 dual-core** with Cortex-M7 as the main core and Cortex-M4 as auxiliary.

## Core Split

| Core | Clock | Role |
|---|---|---|
| Cortex-M7 | 480 MHz | FreeRTOS + LwIP + micro-ROS XRCE-DDS |
| Cortex-M4 | 240 MHz | Auxiliary / future expansion |

Both cores synchronize via **Hardware Semaphores (HSEM)**.

## Software Stack (CM7)

```
FreeRTOS tasks
    └── LwIP (UDP stack)
        └── micro-ROS XRCE-DDS transport
            └── UDP → micro-ROS Agent (host:8888)
```

## Build Output

| File | Purpose |
|---|---|
| `MicroRosEth_CM7.elf` | CM7 debug + symbol file |
| `MicroRosEth_CM7.bin` | CM7 raw binary |
| `MicroRosEth_CM4.elf` | CM4 debug + symbol file |
