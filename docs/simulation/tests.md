---
title: Tests
parent: Simulation
nav_order: 4
---

# Simulation Tests

Robot Framework tests live under `Test_Board_Sensore/simulation/tests/robot`.

## Run Tests

From `Test_Board_Sensore/simulation`:

```
robot tests/robot/test_sensor_ultrasonic.robot
```

The tests parse `RESULT key=value` lines emitted by Renode scripts.

## CI Example

TODO: Add the actual CI YAML once it exists. Example placeholder:

```
name: renode-sim
on: [push]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: sudo apt-get install -y renode robotframework
      - run: make -C Test_Board_Sensore/Makefile/CM4_smoke
      - run: robot Test_Board_Sensore/simulation/tests/robot/test_sensor_ultrasonic.robot
```
