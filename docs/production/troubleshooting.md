---
title: Troubleshooting
parent: Production
nav_order: 4
---

# Production Troubleshooting

## Dashboard shows "waiting_for_nodes"

microk3 starts with an empty node list and waits for live ROS 2 messages. Check:

```bash
# Is Renode running and publishing?
docker-compose logs renode-e2e

# Is the bridge translating heartbeats?
docker-compose logs renode-bridge

# Are topics being published?
docker exec -it <microk3-container> bash
source /opt/ros/humble/setup.bash
ros2 topic echo /microk3/node_status
```

## firmware-build fails

```bash
# Check ARM toolchain is present in the build image
docker-compose run firmware-build arm-none-eabi-gcc --version

# Check libmicroros.a exists
ls microrosWs/Micro_ros_eth/microroseth/micro_ros_stm32cubemx_utils/microros_static_library/libmicroros/libmicroros.a
```

If the file is missing, follow [Building the Library](../../microros/build.html).

## Ethernet DMA issues / firmware hangs at boot

Ensure `SCB_DisableDCache()` is called in `main.c` **before** `HAL_Init()` and that the MPU is configured for SRAM2 as non-cacheable. See [CM7 Core](../../firmware/cm7.html).

## TAP interface not found

```bash
# Renode-e2e needs NET_ADMIN capability and privileged mode
# Verify in compose file:
cap_add:
  - NET_ADMIN
privileged: true
```
