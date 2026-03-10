---
title: Home
nav_order: 1
---

# MICRO_ROS_ETH Documentation

Complete framework for Ethernet-based micro-ROS on STM32H7 dual-core microcontrollers.

## What This Project Does

- Runs a ROS 2 XRCE-DDS client on STM32H7 (Cortex-M7 + FreeRTOS + LwIP)
- Communicates over UDP Ethernet to a micro-ROS agent on the host
- Exposes ROS 2 topics consumed by the **microk3** Flask dashboard
- Supports hardware simulation via Renode (no physical board needed)

## Quick Links

| Section | Description |
|---|---|
| [Firmware](firmware/overview.md) | STM32H7 CM7/CM4 build and flash |
| [micro-ROS](microros/overview.md) | Library build, transport, topics |
| [microk3](microk3/overview.md) | Flask dashboard and REST API |
| [Simulation](simulation/overview.md) | Renode scripts and test runner |
| [Testing](testing/overview.md) | Full test pyramid |
| [Production](production/overview.md) | Jetson Orin NX deployment |
| [Hardware](hardware/reference.md) | Supported boards and wiring |
| [Troubleshooting](troubleshooting.md) | Common issues and fixes |
