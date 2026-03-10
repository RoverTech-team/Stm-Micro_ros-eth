---
title: Home
nav_order: 1
---

# Stm-Micro_ros-eth Docs

This site collects high-level documentation for the STM32H7 micro-ROS Ethernet stack, Renode simulation, and production deployment workflows. It is intentionally concise and links into the most relevant parts of the repo.

## Architecture Overview

```
Host Machine / CI
  |-- micro-ROS agent (UDP:8888)
  |-- microk3 dashboard (:5050)
  |-- Renode (STM32H755 simulation)
           |
           | XRCE-DDS / UDP
           v
STM32H7 (CM7 + CM4)
  |-- LwIP + micro-ROS
  |-- Sensor driver (JSN-SR04T)
```

## Sections

- [Firmware](firmware/index.md)
- [micro-ROS](microros/index.md)
- [Simulation](simulation/index.md)
- [Production](production/index.md)
- [Hardware Reference](hardware/reference.md)
- [Global Troubleshooting](troubleshooting.md)
