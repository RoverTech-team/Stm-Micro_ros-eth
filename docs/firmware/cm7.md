---
title: CM7 Core
parent: Firmware
nav_order: 2
---

# Cortex-M7 Core

The CM7 is the primary core. It runs FreeRTOS, initializes LwIP over Ethernet, and hosts the micro-ROS client.

## Critical Startup Configuration

D-Cache **must** be disabled before Ethernet DMA is used — otherwise DMA descriptor corruption occurs:

```c
// In main.c — call before HAL_Init()
SCB_DisableDCache();
```

## MPU Configuration

The Ethernet DMA descriptor region (SRAM2 at `0x30000000`) must be configured as non-cacheable bufferable:

```c
MPU_Region_InitTypeDef MPU_InitStruct = {0};
MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
MPU_InitStruct.BaseAddress      = 0x30000000;  // SRAM2
MPU_InitStruct.Size             = MPU_REGION_SIZE_32KB;
MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
MPU_InitStruct.IsBufferable     = MPU_ACCESS_BUFFERABLE;
MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
HAL_MPU_ConfigRegion(&MPU_InitStruct);
```

## STM32CubeMX Key Settings

TODO: Add the exact CubeMX settings table once verified in the project.

## Build

```bash
cd microrosWs/Micro_ros_eth/microroseth/Makefile/CM7
make clean && make -j$(nproc)
```
