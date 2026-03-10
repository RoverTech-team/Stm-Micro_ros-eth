---
title: Overview
parent: Production
nav_order: 1
---

# Production Overview

The production stack packages Renode, the micro-ROS agent, and the microk3 dashboard for Jetson Orin NX, with optional HIL mode.

## Mode A: E2E (Renode)

```
Jetson Orin NX
  |-- Renode (STM32H755 sim)
  |-- micro-ROS agent (UDP 8888)
  |-- microk3 dashboard (HTTP 5050)
```

## Mode B: HIL (Hardware)

```
Jetson Orin NX
  |-- micro-ROS agent (UDP 8888)
  |-- microk3 dashboard
  |
  |  Ethernet
  v
STM32H7 hardware board
```

Source: `production/jetson-orin-nx/README.md`
