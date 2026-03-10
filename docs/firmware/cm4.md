---
title: CM4 Core
parent: Firmware
nav_order: 3
---

# Cortex-M4 Core

The CM4 runs at 240 MHz and is reserved for auxiliary processing. It boots after CM7 releases it via HSEM.

## Build

```bash
cd microrosWs/Micro_ros_eth/microroseth/Makefile/CM4
make clean && make -j$(nproc)
```

## Synchronization

CM7 uses Hardware Semaphore **HSEM** to signal CM4 boot readiness. No shared memory is used by default — extend `Common/Src/` for inter-core communication.
