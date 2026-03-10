---
title: Sensor Model
parent: Simulation
nav_order: 3
---

# JSN-SR04T Sensor Model

The JSN-SR04T ultrasonic sensor behavior is implemented in Python and optionally mirrored by a C# Renode plugin.

## C# Plugin

Path: `Test_Board_Sensore/simulation/src/JSN_SR04T_Plugin/JSN_SR04T.cs`

Key behavior:

- Validates trigger pulse width (~10 us)
- Computes echo pulse width as `distance_cm * 58`
- Enforces timeout handling

## Python Helper

Path: `Test_Board_Sensore/simulation/python/sensor_helper.py`

State machine (from the helper):

```
idle -> triggered -> measuring -> ready
  ^        |             |         |
  |        v             v         v
  +----- timeout <---- echo end ----+
```

GPIO bindings (Renode helper):

- ECHO: `PD0`
- TRIG: `PD1`

TODO: Confirm any alternative pin maps for hardware builds.
