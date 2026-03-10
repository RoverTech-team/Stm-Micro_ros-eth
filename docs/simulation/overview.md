---
title: Simulation Overview
parent: Simulation
nav_order: 1
---

# Renode Simulation

The project uses **Renode** to simulate the STM32H7 without physical hardware. Both bare-board and full networked (TAP) modes are supported.

## Two Network Modes

| Mode | Interface | IP range | Use case |
|---|---|---|---|
| Internal | Renode virtual net | TODO | macOS dev, no TAP needed |
| TAP | Linux TAP interface | `192.168.50.x` | CI, Docker, Jetson |

## Script Inventory (`Test_Board_Sensore/simulation/scripts/`)

| Script | Purpose |
|---|---|
| `board_smoke.resc` | Quick boot smoke test (single core) |
| `board_smoke_tap.resc` | Smoke test over TAP interface |
| `board_dual_smoke.resc` | Dual-core (CM7+CM4) smoke test |
| `board_validation.resc` | Single-core full validation |
| `board_dual_validation.resc` | Dual-core full validation |
| `firmware_validation.resc` | Firmware-only validation (no network) |
| `microroseth_validation.resc` | Full micro-ROS Ethernet validation |
| `microroseth_validation_tap.resc` | micro-ROS validation over TAP |
| `run_simulation.resc` | General-purpose simulation runner |
| `sensor_test.resc` | Sensor board test |
| `run_microk3_host_tap.sh` | Start TAP + agent + Renode + microk3 |
