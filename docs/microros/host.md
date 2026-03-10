---
title: Running the Agent
parent: micro-ROS
nav_order: 4
---

# Running the micro-ROS Agent (Host)

The agent bridges XRCE-DDS (UDP) from the STM32 to ROS 2 DDS on the host.

## Via Docker (recommended)

```bash
docker run -it --rm \
  --net=host \
  microros/micro-ros-agent:humble \
  udp4 --port 8888
```

## Via docker-compose (microk3 stack)

The `microk3/docker-compose.yml` includes the agent as a service. Run:

```bash
cd microrosWs/microk3
docker-compose up
```

## Network Requirements

| Item | Value |
|---|---|
| Agent IP | `192.168.50.1` |
| Agent port | `8888` (UDP) |
| STM32 IP | `192.168.50.2` |
| Subnet | `192.168.50.0/24` |

> In Renode/TAP mode the subnet shifts to `192.168.50.0/24` via environment variable `TAP_GATEWAY_CIDR`.
