---
title: Build
parent: micro-ROS
nav_order: 2
---

# Build and Flash

This repo uses GNU ARM toolchains and make-based builds for STM32H7 firmware.

## Toolchain

- ARM GCC: `arm-none-eabi-gcc` (required for CM4/CM7 builds)
- CMake: used for host-side tooling/tests where applicable

## Build Targets

Common targets in this repo include:

- `Test_Board_Sensore/Makefile/CM4_smoke`
- `Test_Board_Sensore/Makefile/CM7_dual_smoke`
- `Test_Board_Sensore/Makefile/CM4_dual_smoke`
- `microrosWs/Micro_ros_eth/microroseth/Makefile/CM7`

Example build:

```
make -C Test_Board_Sensore/Makefile/CM4_smoke
```

## Flash Commands

TODO: Add the canonical flashing command for your environment.

Common approaches found in the repo include STM32CubeProgrammer CLI and SWD flashing workflows. See `microrosWs/Micro_ros_eth/testing.md` for command examples.
