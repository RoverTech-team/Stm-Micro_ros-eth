---
title: Host + Agent
parent: micro-ROS
nav_order: 4
---

# Host + Agent

The host runs the micro-ROS agent and optionally a TAP bridge for Renode simulation.

## TAP Setup

From `Test_Board_Sensore/simulation/README.md` and E2E docs:

- Host TAP: `tap0` at `192.168.50.1/24`
- Renode STM32: `192.168.50.2/24`
- Agent port: `8888/udp`

TODO: Add the exact host-side TAP creation commands for macOS/Linux.

## Agent Commands

Common agent command used in tests:

```
micro-ros-agent udp4 --port 8888 -v6
```

In Docker (from `microrosWs/microk3/docker-compose.yml`):

```
microros/micro-ros-agent:humble udp4 --port 8888
```
