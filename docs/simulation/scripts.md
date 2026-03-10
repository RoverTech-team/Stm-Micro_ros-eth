---
title: Scripts
parent: Simulation
nav_order: 2
---

# Renode Scripts

The main simulation scripts live in `Test_Board_Sensore/simulation/scripts`.

## Script List

- `board_smoke.resc`
- `board_smoke_tap.resc`
- `board_validation.resc`
- `board_dual_smoke.resc`
- `board_dual_validation.resc`
- `firmware_validation.resc`
- `sensor_test.resc`
- `run_simulation.resc`
- `microroseth_validation.resc`
- `microroseth_validation_tap.resc`

Note: The repo currently contains 10 scripts in this folder; the original plan mentioned 9. TODO: Confirm the canonical subset and update this list.

## Selection Guide

- Use `board_smoke.resc` for interactive bring-up.
- Use `board_dual_smoke.resc` for CM7-first dual-core validation.
- Use `microroseth_validation*.resc` for Ethernet/XRCE-DDS flows.
- Use `sensor_test.resc` for deterministic sensor checks.
