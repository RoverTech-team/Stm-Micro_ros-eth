# ARM main-board simulation (PCB models + ROS) — Renode environment

Models **every element** of the arm-controller carrier PCB
(`docs/pcb_elements.md`, `docs/img/`) and connects the simulated
firmware to ROS exactly like hardware: micro-ROS XRCE-DDS over
Ethernet to a `uros-agent` (UDP `8888`), ROS topics on the host.

```
STM32H755 (CM7+CM4, shared SRAM4) ──ETH──► agent:8888 ──► ROS topics
  │ CM7: /heartbeat /joint_states /joint_commands   (BRINGUP firmware)
  │ CM4: DRIVE_SPI (powerSTEP01 x6) / brakes J2+J3 / shared SRAM4
  ├── ENC/1 + ENC/2 (5x quadrature 1000PPR, J6 unpopulated)
  ├── IMU_SPI (2x IMU, CS1/CS2, IMU2_NENABLE=LOW)   [fw stub, ROS planned]
  └── 24-48V + 5V power domains                     [fault hooks]
```

## Layout

| path | content |
|---|---|
| `platforms/arm_main_pcb.repl` | dual H755 + ETH/PHY + SPI1 (drives) + SPI2 (IMU) + TIM3/4/5 (encoders) + 5x encoder + 2x IMU + 1x driver-chain models |
| `peripherals/PowerStep01Chain.cs` | 6x powerSTEP01 daisy chain (SET/GET_PARAM, MOVE/RUN/GOTO/STOP, GET_STATUS, ABS_POS motion integration, UVLO/HiZ + blown-fuse fault hook) |
| `peripherals/QuadratureEncoder.cs` | 1000PPR A/B/Z follower (4000 cnt/rev), MMIO + Python `Advance` |
| `peripherals/SpiImu.cs` | 2x SPI IMU stub (WHO_AM_I, accel/gyro regs, Python `FeedAccel`) |
| `scripts/step1_echo.resc` | BRINGUP step 1 (`cm7_echo` + `cm4_follow`): net + echo, no motion |
| `scripts/step2_e2e.resc` | BRINGUP step 2 (`cm7_e2e` + `cm4_follow`): J1–J5 shared, J6 held |
| `scripts/step3_full.resc` | BRINGUP step 3 (`cm7_e2e_full` + `cm4_full`): all 6, attended |
| `scripts/run_sim.sh` | launcher (headless/CI/timeout/GUI, like `tests/simulation`) |
| `tests/test_arm_board_boot.robot` | platform + WHO_AM_I + encoder + driver-status checks |
| `tests/test_arm_board_e2e.robot` | agent-backed XRCE checks (needs TAP/docker, like `tests/e2e`) |
| `ros/arm_board_bridge.py` | host bridge: Float32MultiArray→JointState, sim /imu/data_raw, /diagnostics |
| `docs/pcb_elements.md` | element → pins → model → ROS-topic matrix distilled from `Main_PCB_Documentation.docx` |

## Quick start (no root, no agent)

```bash
# 1. build firmware (from repo root; single-source BRINGUP, bit-identical ELFs)
make -C microrosWs/Micro_ros_eth/microroseth/BRINGUP/cm7_echo -j4
make -C microrosWs/Micro_ros_eth/microroseth/BRINGUP/cm4_follow -j4
# 2. boot step 1 in Renode headless (20 s virtual, UART3 asserts)
microrosWs/tests/arm_board/scripts/run_sim.sh --step step1 --headless --timeout 60
```

Expected: `CM7: hal-clock-init-ok`, `CM7: joint-poll-start`,
UART3 `CM7: joint-cmd-recv` after agent traffic (step 1 needs the
agent for XRCE; boot lines appear without it).

## With ROS (TAP + agent, needs sudo/docker — same pattern as `tests/e2e`)

```bash
sudo microrosWs/tests/arm_board/scripts/run_sim.sh --step step2 --tap
docker run --rm --net host microros/micro-ros-agent:humble udp4 --port 8888
ros2 topic list   # /heartbeat /joint_states /joint_commands (+ bridge: /joint_states_norm /imu/data_raw /diagnostics)
```

## Coverage matrix (v1)

| PCB element | model | verified |
|---|---|---|
| H755 CM7+CM4, flash banks, SRAM incl. SRAM4 `0x38000000` | `arm_main_pcb.repl` (stock + tags) | boot robot |
| ETH MAC + LAN8742 (`0x0007/0xC130`) | `SynopsysDWCEthernetQualityOfService` + switch | ping via TAP |
| 6x powerSTEP01, DRIVE_SPI=SPI1 (PA5/PA6/PB5, CS PD14, RST PG12) | `PowerStep01Chain.cs` | status/pos/move robot |
| 5x encoder 1000PPR, ENC/1 (J20) ENC/2 (J21), J6 empty | `QuadratureEncoder.cs` x5 | advance/count robot |
| 2x IMU SPI (J22: CS1/CS2/SCK/MOSI/MISO, NENABLE=LOW) | `SpiImu.cs` x2 | WHO_AM_I robot |
| Brakes J2 (PE11) J3 (PA8), BRK-NTC/BRAKES conn | real GPIO pins, robot asserts | boot robot |
| 24-48V/5V rails, fuse (UVLO/HiZ latch) | `FaultBlownFuse` hook on driver chain | e2e (fault) |
| ROS topics | agent (external) + `ros/arm_board_bridge.py` | e2e |
| NTC analog, current sense, 5V-rail load | **not modeled** (no ADC in scope) | — |

Restore/rollback: BRINGUP firmware is backed up under `backups/`
(see `BRINGUP/README.md`); sim files are additive only — nothing
outside `microrosWs/tests/arm_board/` is modified.

## Troubleshooting (verified on Renode 1.16.1)

- **No UART text in sim**: `STM32F7_USART` TXE reads 0, so
  `HAL_UART_Transmit` times out silently (same on hardware it prints).
  Asserts read sim state (`shared_magic`, `motor_ready`, encoder MMIO)
  instead — see `scripts/step1_echo.resc` tail.
- **`//` comments are fatal** in `.resc` on this Renode (tokenize
  error aborts the script). Use `#`.
- **`monitor.Parse` returns bool** here — read state via
  `emulationManager.CurrentEmulation` + `SystemBus.ReadDoubleWord`
  (see assert block), not `Parse` strings.
- **SPI-attached models** (`drivers`, `imuN`) are not monitor-addressable
  by name; `drivers` health is proven by `motor_ready=1` (CM4's own
  `GetStatus` path), IMU by attach + future firmware reads.
- **IMU2 is on SPI3 in sim** (one slave per SPI bus instance), shared
  J22 bus on PCB — same wire protocol, documented in
  `docs/pcb_elements.md §4`.
- **Sim wall-time varies wildly** (DWT spins): keep headless timeouts
  ≥240 s; `sleep 12` virtual is the CM4-test settle window.
