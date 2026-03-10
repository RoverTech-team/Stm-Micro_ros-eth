---
title: micro-ROS Overview
parent: micro-ROS
nav_order: 1
---

# micro-ROS Overview

The firmware implements a micro-ROS **XRCE-DDS client** over UDP/Ethernet. It connects to a micro-ROS agent running on the host at `192.168.50.1:8888`.

## Data Flow

```
STM32H7 (192.168.50.2)
  └── FreeRTOS task
      └── micro-ROS executor
          └── XRCE-DDS over UDP
              └── micro-ROS Agent (192.168.50.1:8888)
                  └── ROS 2 topics → microk3 dashboard
```

## RMW Configuration

| Parameter | Value |
|---|---|
| Max nodes | 1 |
| Max publishers | 5 |
| Max subscriptions | 5 |
| Max services | 1 |
| Max history | 4 |
| Transport | Custom (UDP over LwIP) |
| Serial profile | Disabled |
