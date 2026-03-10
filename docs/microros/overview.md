---
title: Overview
parent: micro-ROS
nav_order: 1
---

# micro-ROS Overview

The micro-ROS stack targets STM32H7 dual-core MCUs with LwIP and XRCE-DDS over UDP. It integrates with the host micro-ROS agent and the `microk3` dashboard.

## Network Architecture

- Firmware publishes XRCE-DDS packets over UDP to the agent (default port `8888`).
- The agent forwards topics into ROS 2.
- `microk3` and the Renode heartbeat bridge subscribe/publish on ROS 2 topics.

## HIL vs Simulation

- HIL: STM32 board connected over Ethernet to the host/Jetson running the agent and dashboard.
- Simulation: Renode runs the STM32 image, optionally bridged to host TAP (`tap0`).

Sources:
- `microrosWs/README.md`
- `microrosWs/tests/e2e/README.md`
