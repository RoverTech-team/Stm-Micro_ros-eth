---
title: Overview
parent: Simulation
nav_order: 1
---

# Renode Overview

The simulation stack lives under `Test_Board_Sensore/simulation` and provides:

- Renode platform models for STM32H755
- JSN-SR04T sensor model (Python helper + optional C# plugin)
- A suite of Renode scripts for smoke, validation, and micro-ROS testing

## Platform Files

From `Test_Board_Sensore/simulation/platform`:

- `nucleo_h755zi_q.repl`
- `nucleo_h755zi_q_dual.repl`
- `stm32h755_with_sensors.repl`

## Quick Start

Example runs from `Test_Board_Sensore/simulation/README.md`:

```
make -C ../Makefile/CM4_smoke
make -C ../Makefile/CM7_dual_smoke
make -C ../Makefile/CM4_dual_smoke
$RENODE_PATH scripts/board_smoke.resc
```

TODO: Add any Renode CLI flags or headless mode details used in CI.
