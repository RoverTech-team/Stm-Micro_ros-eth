---
title: Troubleshooting
nav_order: 10
---

# Troubleshooting

> **Quick Jump:**
> [libmicroros.a not found](#libmicrorosa-not-found) ·
> [Ethernet DMA hang](#ethernet-dma-corruption--firmware-hangs) ·
> [Agent not connecting](#micro-ros-agent-not-connecting) ·
> [ROS import fails](#microk3-ros-2-import-fails-at-startup) ·
> [Rate limit 429](#rate-limit-429-errors-on-api)

---

<span class="severity severity-warning">WARNING</span>
## libmicroros.a not found — firmware build fails

The static library must be built manually. Follow [Building the Library](microros/build.html) step by step. The file must land at:
```
microrosWs/Micro_ros_eth/microroseth/micro_ros_stm32cubemx_utils/microros_static_library/libmicroros/libmicroros.a
```

<span class="severity severity-critical">CRITICAL</span>
## Ethernet DMA corruption / firmware hangs

D-Cache **must** be disabled before `HAL_Init()` in `main.c`. The MPU must mark SRAM2 (`0x30000000`, 32 KB) as non-cacheable bufferable. See [CM7 Core](firmware/cm7.html).

<span class="severity severity-warning">WARNING</span>
## micro-ROS agent not connecting

- Verify STM32 IP is `192.168.50.2` and agent is on `192.168.50.1:8888`
- In TAP/Docker mode verify `TAP_GATEWAY_CIDR=192.168.50.1/24` and agent binds `0.0.0.0:8888`
- Check `MEMP_NUM_UDP_PCB` is set to 15 in LwIP config

<span class="severity severity-info">INFO</span>
## microk3 ROS 2 import fails at startup

If `ros_interface.py` fails to import, microk3 still starts but without ROS — it shows a warning and runs in API-only mode. Install `rclpy` or use the Docker image which includes ROS 2 Humble.

<span class="severity severity-info">INFO</span>
## Rate limit 429 errors on API

Default limits: 30/min (read), 10/min (write). Increase via `RATELIMIT_DEFAULT` env var in `.env`.
