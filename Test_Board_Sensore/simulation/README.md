# JSN-SR04T Ultrasonic Sensor Simulation

This directory contains the Renode simulation stack for STM32H755 CM4 firmware,
board bring-up, and JSN-SR04T (AJ-SR04M compatible) ultrasonic sensor behavior.

## What Is Implemented

- Shared `NUCLEO-H755ZI-Q` board models for CM4-only and dual-core bring-up
- CM4 UART smoke firmware and CM7/CM4 dual-core smoke firmware
- Ethernet-ready board wiring via Renode switch / optional TAP bridge
- Complete sensor state machine in Python (`idle`, `triggered`, `measuring`, `timeout`, `ready`)
- Distance clamping and timeout handling
- Echo pulse-width computation (`distance_cm * 58`)
- Deterministic Renode script outputs via `RESULT key=value`
- Robot tests that assert real script outputs (no placeholder keywords)

## Layout

```
simulation/
|-- config/
|   `-- sensor_config.yaml
|-- platform/
|   |-- nucleo_h755zi_q.repl
|   |-- nucleo_h755zi_q_dual.repl
|   `-- stm32h755_with_sensors.repl
|-- python/
|   `-- sensor_helper.py
|-- scripts/
|   |-- board_dual_smoke.resc
|   |-- board_dual_validation.resc
|   |-- board_smoke.resc
|   |-- board_smoke_tap.resc
|   |-- board_validation.resc
|   |-- microroseth_validation.resc
|   |-- microroseth_validation_tap.resc
|   |-- run_simulation.resc
|   `-- run_microk3_host_tap.sh
|   `-- sensor_test.resc
|-- src/
|   |-- JSN_SR04T.cs
|   `-- JSN_SR04T_Plugin/
`-- tests/
    `-- robot/
        |-- common.robot
        `-- test_sensor_ultrasonic.robot
```

## Prerequisites

- Renode executable available either:
  - via `RENODE_PATH` env var, or
  - at `../Renode.app/Contents/MacOS/renode` relative to `simulation/`
- Robot Framework installed for automated tests (`robot` command)
- CM4 ELF at `../CM4/build/Polispace_Stm_CM4.elf` for integration run
- ARM GCC toolchain (`arm-none-eabi-gcc`) for `../Makefile/CM4_smoke`,
  `../Makefile/CM7_dual_smoke`, and `../Makefile/CM4_dual_smoke`

## Run

From `simulation/`:

```bash
$ make -C ../Makefile/CM4_smoke
$ make -C ../Makefile/CM7_dual_smoke
$ make -C ../Makefile/CM4_dual_smoke
$RENODE_PATH scripts/board_smoke.resc
$RENODE_PATH scripts/board_dual_smoke.resc
$RENODE_PATH scripts/board_smoke_tap.resc
$RENODE_PATH scripts/microroseth_validation.resc
$RENODE_PATH scripts/microroseth_validation_tap.resc
$RENODE_PATH scripts/sensor_test.resc
$RENODE_PATH scripts/run_simulation.resc
```

To link the Renode STM32H755 simulation to the `microk3` dashboard and
micro-ROS agent on the host:

```bash
bash scripts/run_microk3_host_tap.sh
```

That flow assumes:

- host TAP `tap0` at `192.168.50.1/24`
- Renode STM32H755 at `192.168.50.2/24`
- `microk3` Docker stack exposing the agent on `192.168.50.1:8888`

## Test

From `simulation/`:

```bash
robot tests/robot/test_sensor_ultrasonic.robot
```

The test suite parses `RESULT key=value` lines produced by Renode scripts.

## Notes

- `sensor_test.resc` validates the sensor model deterministically.
- `board_smoke.resc` is the primary interactive board bring-up entry point.
- `board_dual_smoke.resc` is the CM7-first dual-core bring-up entry point.
- `board_validation.resc` mirrors USART3 TX to the Renode console log for automation.
- `board_dual_validation.resc` mirrors both CM7 and CM4 UART traffic to the Renode
  console log while validating the HSEM-driven release path.
- `microroseth_validation.resc` is the isolated Ethernet smoke path with no external
  agent connectivity.
- `microroseth_validation_tap.resc` is the end-to-end path for host TAP integration.
- `run_simulation.resc` loads platform + firmware + helper for integration execution.
