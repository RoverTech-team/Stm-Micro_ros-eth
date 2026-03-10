---
title: CM7 Core
parent: Firmware
nav_order: 2
---

# Cortex-M7 Core
{: .no_toc }

The CM7 is the primary core. It runs FreeRTOS, initializes LwIP over Ethernet, and hosts the micro-ROS client.

---

## Critical Startup Configuration
{: .fs-6 }

D-Cache **must** be disabled before Ethernet DMA is used — otherwise DMA descriptor corruption occurs:

> [!CAUTION]
> If D-Cache is enabled for the DMA memory region, the Ethernet peripheral may read stale data or fail to update descriptors correctly.

{% include code_label.html label="main.c" %}
```c
// In main.c — call before HAL_Init()
SCB_DisableDCache();
```

## MPU Configuration
{: .fs-6 }

The Ethernet DMA descriptor region (SRAM2 at `0x30000000`) must be configured as **non-cacheable bufferable** to ensure consistency between the CPU and the Ethernet MAC.

{% include code_label.html label="MPU Config" %}
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
{: .fs-6 }

| Peripheral | Setting | Value |
|---|---|---|
| **ETH** | Mode | RMII |
| **ETH** | RX Mode | Polling/Interrupt |
| **LWIP** | DHCP | Disabled (Static IP) |
| **FREERTOS** | CMSIS_V2 | Enabled |

---

## Build Commands
{: .fs-6 }

To compile the CM7 firmware binary:

```bash
cd microrosWs/Micro_ros_eth/microroseth/Makefile/CM7
make clean && make -j$(nproc)
```

> [!NOTE]
> Ensure the `libmicroros.a` is already present in the expected utility folder before building.
