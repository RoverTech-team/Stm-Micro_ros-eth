---
title: Running Scripts
parent: Simulation
nav_order: 2
---

# Running Renode Scripts

## macOS (Renode.app)

```bash
# Full micro-ROS validation (internal network)
Renode.app/Contents/MacOS/renode \
  Test_Board_Sensore/simulation/scripts/microroseth_validation.resc

# Dual-core smoke test
Renode.app/Contents/MacOS/renode \
  Test_Board_Sensore/simulation/scripts/board_dual_smoke.resc
```

## Linux

```bash
# TAP-based validation (requires root for TAP setup)
sudo renode Test_Board_Sensore/simulation/scripts/microroseth_validation_tap.resc
```

## Full Stack with microk3 (TAP mode)

```bash
# Sets up TAP, starts micro-ROS agent, Renode, and microk3 dashboard
cd Test_Board_Sensore/simulation/scripts
chmod +x run_microk3_host_tap.sh
sudo ./run_microk3_host_tap.sh
```
