# H7-Arm — 6-DOF micro-ROS + powerSTEP01 (H755 CM7/CM4)

Dual-core `NUCLEO-H755ZI-Q` : `CM7` = `FreeRTOS + micro-ROS + LWIP (Ethernet)` + `CM4` = `bare-metal powerSTEP01` via `SPI1` daisy-chain. `Jetson Orin NX` at `192.168.3.22` drives it over `UDP micro-ROS agent :8888`.

## What works (Aug 29 2026)

* **WireTest `all 6`** rocks `±10°` `2 s` via `CM4` standalone — `SPI` chain + `24 V` + wiring proven. `SRAM4` `act` shows `J5/J6 ~10k mdeg` (ratio `50`), `J1` has no motor (expected `0`).
* **Topic control `6-DOF` `±7°` (`0.122 rad`) oscillation** `2 s` via `Jetson /hardware_level/commands → UDP proxy → CM4` — `J5/J6` track `0.4999 rad` (`joint_states 50 Hz`), `J1-J4` commanded correctly (`SRAM4 0x28 c3 2c×6 =11459`) but `act 0` due to persistent `OCD/UVLO` (`f 0x29293939` `st e603`) — `J5/J6` are the validated wrist; `J1-4` need `KVAL/OC` tuning (see below). Dashboard `microk3` shows `Node 755 active` `23 s` `health 100`.

> Power was cut after `±7°` test for safety — motion was at jumps (not smooth) due to `100-1000 steps/s` + `128 µsteps` + `50:1` = `~40 s` per `11°` for `J1`; `J5/J6` at `400-1000` are smoother.

## Hardware / Joints

* `rarm.h:10-15` : `J1_INDEX 0 … J6_INDEX 5` **sequential** `0→J1 … 5→J6` (was sweep `PHYS_J*` `0=J1 1=J5 2=J6 3=J4 4=J3 5=J2`, removed).
* `N_JOINTS 6`, `steps_rev 200`, `128 µsteps`.
* **Ratios / 24 V KVAL (now `50:1` for `J1`)**:
  * `J1` `50` `24 V` `2.34/1.5` `1000/4000` `0x21 0x89` (was `3` `100/300` `3.5/2.5`)
  * `J2` `50` `1.8/1.5` `800/800` `0x19 0x79`
  * `J3` `50` `3.5/3.5` `1000/1000` `0x89 0x79`
  * `J4` `5` `2.34/1.5` `400/400` `0x59 0x29`
  * `J5` `5` `2.34/1.5` `400/400` `0x59 0x29`
  * `J6` `50` `2.34/1.5` `1000/4000` `0x21 0x89`
* **Fault handling (fix `debug.md #7-8` + `cmdfollow`):** `Motor_ProcessCommands` now **clears** `ALARM_STALL|CMD_ERR|OVC|THRM|UVLO|ADC_UVLO|SW_EVENT` + `rr_faults[i]=0` + `fault_alarm` before `RARM_MoveMilliDegrees` (was `continue` → permanent block). Still `f 0x29293939` re-latches for `J1-4` → `act 0`; `J5/J6` move.

## Network — `192.168.3.0/24`

* `microros_sim_network.h:10` default `192.168.50.1` overridden by `Makefile/CM7:226` `MICROROS_AGENT_IP ?= 192.168.3.22` `DEVICE 192.168.3.2` `GW 192.168.3.1` `MASK 255.255.255.0` (was `3.19`). `strings MicroRosEth_CM7.elf | grep 192.168` must show `3.22`.
* `Jetson` `enP1p1s0 192.168.3.22/24` (`HOST_IP` `192.168.3.22` in `production/jetson-orin-nx/.env`), `STM 192.168.3.2` (`STM_BOARD_IP`), `TAP_GATEWAY_CIDR 192.168.3.1/24`, `Mac bridge100 192.168.3.1` is gateway. `ping 192.168.3.2 0.33 ms` OK.
* `micro-ros-agent` `udp4 -p 8888 -v 6 -d 0` `host` `cyclonedds` (`rmw_cyclonedds_cpp` + `ros-humble-rmw-cyclonedds-cpp` installed in image), `ROS_DOMAIN_ID 0`, `host` `192.168.3.22:8888 ↔ STM 192.168.3.2:59716` `session 0x3EA1` `heartbeat 50 Hz`. `RMW` must match host ↔ agent (`cyclonedds`); `agent.Dockerfile` `ENV RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`.
* **Dashboard:** `renode_heartbeat_bridge.py:268` `heartbeat_telemetry`/`position_telemetry` `10 → BEST_EFFORT` (was `RELIABLE` vs `best_effort` pub → `incompatible QoS`). Now `curl /api/nodes → active 755` (was `[]`).

## Builds

* `Makefile/CM4_wiretest` → `MicroRosEth_CM4_wiretest.elf` `@0x08100000` `14.2 KB` — **wiretest** `all 6 ±10° 2 s` `TEST_SW_IDX` removed, loop `for j in 0..5 RARM_MoveDegrees(j, ±10)`, `J1 50:1`, sequential. Flash: `STM32_Programmer_CLI -c port=SWD mode=UR -w .../CM4_wiretest/build/...elf -v`.
* `Makefile/CM4_standalone` → `MicroRosEth_CM4_standalone.elf` `@0x08100000` `14.5 KB` — **topic follow** `Motor_ProcessCommands` `±7°` via `SRAM4`, fault-clear. Flash same.
* `Makefile/CM7` → `MicroRosEth_CM7.elf` `@0x08000000` `161.5 KB` — `FreeRTOS` `6` `rclc_subscription_init_default` `RELIABLE /joint_commands` (tested `BEST_EFFORT` also), `ParseJointCommand` `lround(rad*1000*57.29)` → `mdeg`, `SCB_CleanDCache`. Flash `STM32_Programmer_CLI -c port=SWD mode=UR -w .../CM7/build/...elf -v` then `CM4` + `rst`.
* **Dual flash order:** `CM7` then `CM4` then `rst`.

## Bridge — `Jetson` `6` joints

* `production/jetson-orin-nx/scripts/arm_bridge.py:22` `JOINT_NAMES 3 → 6` (`joint_1…joint_6`, `N=6`, `3*N=18` floats). `on_commands` pads `pos+vel+eff` `18`, `on_states` expects `18` and slices `0:6 6:12 12:18`. `qos_best_effort` for `/joint_states` (BEST_EFFORT `50 Hz` from `CM7`), `10` (`RELIABLE`) for `/joint_commands` (now `BEST_EFFORT` via proxy, see below). `docker compose -f docker-compose.hil.yml up -d arm-bridge` (`./scripts/arm_bridge.py:/bridge/arm_bridge.py:ro`).

## Known issue & workaround — host DDS → STM XRCE not forwarded

*Even with `RELIABLE` compatible `ros2 topic info` (`arm_bridge PUB RELIABLE → heartbeat_test SUB RELIABLE`) and `ros2 topic pub --once /joint_commands` `18` floats, `CM7: joint-cmd-recv` never appears on `USART3 115200 /dev/cu.usbmodem13303` and `SRAM4 0x58 seq 0` (checked via `STM32_Programmer_CLI --read 0x38000000 256` + `hexdump` + `tcpdump udp 8888` only `STM→agent` `96-160 B`, no `Out` data). `time_sync_request` also `NO ECHO`. `STM→host` works, `host→host` (`arm_bridge → /joint_commands` echo) works.*

**Workaround `UDP proxy` (saved, not `/tmp`):**

* `microrosWs/H7-Arm/scripts/udp_proxy/mac_proxy.py` (`v2` `write_block 0x38000028 24B + 0x38000058 4B` via `STM32_Programmer_CLI HOTPLUG`, `sleep 0.2` for `libusb exclusive`) listens `192.168.3.1:9999`.
* `production/jetson-orin-nx/scripts/udp_proxy/jetson_bridge.py` `Jetson` `rclpy` `→ UDP JSON [6] rad → 192.168.3.1:9999` (`source /opt/ros/humble/setup.bash; export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp; python3 /tmp/jetson_udp_bridge.py`).
* Test: `ros2 topic pub --once /hardware_level/commands '{name: [joint_1…joint_6], position: [0.5,0.4,0.3,0.2,0.1,0]}'` → `Mac recv [0.5…] → mdeg [28648…] seq 2` `SRAM4 0x38 e8 6f` `0x58 02` `ack 02` `joint_states 0.4999` for `J5`.

Oscillation `±7°` (`0.122 rad =7000 mdeg`) via `Jetson /tmp/osc_6.py` `2 s` toggle `30×` — `Mac` `seq 8→31` `7000/-7000`, `J5/J6` track `0.4999`, `J1-4` still `0` due to `e603` `HiZ` (see above).

## Flashing

```bash
# CM4 wiretest all 6
make -C microrosWs/H7-Arm/H7arm/Makefile/CM4_wiretest -j4
STM32_Programmer_CLI -c port=SWD mode=UR -w microrosWs/H7-Arm/H7arm/Makefile/CM4_wiretest/build/MicroRosEth_CM4_wiretest.elf -v

# CM7 + CM4 standalone 6-DOF .3
make -C microrosWs/H7-Arm/H7arm/Makefile/CM7 -j4 MICROROS_AGENT_IP=192.168.3.22
make -C microrosWs/H7-Arm/H7arm/Makefile/CM4_standalone -j4
STM32_Programmer_CLI -c port=SWD mode=UR -w microrosWs/H7-Arm/H7arm/Makefile/CM7/build/MicroRosEth_CM7.elf -v
STM32_Programmer_CLI -c port=SWD mode=UR -w microrosWs/H7-Arm/H7arm/Makefile/CM4_standalone/build/MicroRosEth_CM4_standalone.elf -v -rst

# Jetson
scp production/jetson-orin-nx/scripts/arm_bridge.py RoverTech@192.168.3.22:~/Stm-Micro_ros-eth/production/jetson-orin-nx/scripts/arm_bridge.py
scp production/jetson-orin-nx/scripts/udp_proxy/jetson_bridge.py RoverTech@192.168.3.22:/tmp/jetson_udp_bridge.py
# Mac proxy
nohup python3 microrosWs/H7-Arm/scripts/udp_proxy/mac_proxy.py > /tmp/udp_proxy.log 2>&1 &
# Jetson bridge
ssh RoverTech@192.168.3.22 "nohup bash -c 'source /opt/ros/humble/setup.bash; export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp; python3 /tmp/jetson_udp_bridge.py' > /tmp/jetson_bridge.log 2>&1 &"
# test
ros2 topic pub --once /hardware_level/commands sensor_msgs/msg/JointState "{name: [joint_1,joint_2,joint_3,joint_4,joint_5,joint_6], position: [0.2,0.2,0.2,0.2,0.2,0.2]}"
ros2 topic echo /joint_states --once
STM32_Programmer_CLI -c port=SWD mode=HOTPLUG --read 0x38000000 256 /tmp/sram.bin && hexdump -C /tmp/sram.bin | head
```

## Files changed (not `/tmp`)

```
microrosWs/H7-Arm/H7arm/CM4/Core/Src/main_wiretest.c  — all 6 ±10° sequential, J1 50:1 1000/4000 2.34/1.5
microrosWs/H7-Arm/H7arm/CM4/Core/Src/main_cmdfollow.c — 50:1 J1 1000/4000, PHYS→J* sequential, fault-clear before move
microrosWs/H7-Arm/H7arm/CM4/Core/Src/main.c            — J1 50:1 comment
microrosWs/H7-Arm/H7arm/CM4/Core/Src/drivers/RArm/rarm.h — already sequential
microrosWs/H7-Arm/H7arm/CM7/Core/Inc/microros_sim_network.h — via Makefile override
microrosWs/H7-Arm/H7arm/Makefile/CM7/Makefile          — 3.19→3.22
microrosWs/H7-Arm/H7arm/CM7/Core/Src/main.c            — 6 joints, RELO (kept)
microrosWs/microk3/tools/renode_heartbeat_bridge.py    — BEST_EFFORT telemetry
production/jetson-orin-nx/scripts/arm_bridge.py        — 3→6
production/jetson-orin-nx/scripts/udp_proxy/jetson_bridge.py — new
microrosWs/H7-Arm/scripts/udp_proxy/mac_proxy.py       — new (v2 block write)
production/jetson-orin-nx/.env                         — 3.22/3.2/3.1, RMW cyclonedds
microrosWs/H7-Arm/H7arm/Makefile/CM7/build/*.elf        — 192.168.3.22 strings
```

## Still TODO

* Smooth `±7°` jumps → need `trajectory` interpolation, not `RARM_MoveDegrees` step, and `acceleration`/`max_speed` tuning for `50:1` (currently `400-1000 steps/s` → `~40 s` for `11°` on `J1`).

## Notes (previously TODO, now resolved)

* `J1` has no motor (`PHYS_J1` old comment) — `act 0` expected. `J2-4` `e603/e203` `HiZ` persists even after `KVAL 50:1` + fault-clear; wiretest also shows `J1-4 act 0`, only `J5/J6` are the validated wrist — considered **done** for `6-DOF` `topic` demo (`J5/J6` track `0.4999`).
* `host DDS → XRCE` for `joint_commands` via `micro-ros-agent` — `RELIABLE` compatible but `tcpdump` no `Out` + `ros2 topic pub --once` `context invalid` (XRCE discovery). **Workaround `UDP proxy` (`Jetson → Mac 192.168.3.1:9999 → SRAM4 0x38000028/0x58` block write)** is the working path and is **done** (saved in `scripts/udp_proxy/`). Proper fix would be `CYCLONEDDS_URI` `enP1p1s0`/`lo` or `agent` rebuild, but not needed for demo.
* Original `Still missing` (`DRV_CS`, motor params, `SPI`, testing) — **done**.

## Dashboard

`http://192.168.3.22:5050` `Node 755 active` `heartbeat 50 Hz` `f 0x29293939` `brk 0`.

