# ADDITIONAL CONTEXT — live validation notebook

Write here every useful fact confirmed on production. Mark each entry
`[UNVALIDATED]` (from bench reasoning) or `[VALIDATED yyyy-mm-dd]` (observed).
Do not delete rows — append dated notes instead.

## 1. Step status

| step | status | date | note |
|------|--------|------|------|
| step1 echo/net | [PARTIAL] | 2026-09-05 | ping+heartbeat OK; echo uplink works only with `rcl_take` poll fw (`cm7_echo_be`), NOT with `rclc_executor_spin_some` |
| step2 J1–J5 shared | [VALIDATED] | 2026-09-05 | clean-slate ritual (agent restart + STM reset) → seq=1,2,3 all landed (`mdeg=±9998`, ack `0→1→2`); **arm moving on hardware (eyewitness)** |
| step1 echo/net | [VALIDATED] | 2026-09-05 | ping 0% + heartbeat + echo uplink via `rcl_take` poll fw (see issues log) |
| step3 full incl. J6 | [UNVALIDATED] | — | attended, `make step3` |

## 2. Network facts (Jetson ↔ STM, rover LAN)

| fact | value | status |
|------|-------|--------|
| STM CM7 | `192.168.0.5`, GW `192.168.0.1`, MASQ `255.255.255.0`, MAC `00:80:e1:00:00:00` | VALIDATED 2026-11-04 (ping 0.5 ms, agent session live, `/heartbeat` counting) |
| Baked at build time | `MICROROS_DEVICE_IP_*` / `MICROROS_AGENT_IP` Makefile vars (defaults now `192.168.0.5` / `192.168.0.1`) | plain `make` reproduces the flashed image |
| Old direct link | Jetson dongle `enx4865ee1839b8` `192.170.0.1/24` ↔ STM `192.170.0.2` | **retired** — dongle not present, `enP8p1s0` no carrier |
| Agent | `udp4 -p 8888`, bridge on Jetson `0.0.0.0:8888` | VALIDATED (container `uros-agent`) |
| Production agent | `jetson-orin-nx-micro-ros-agent-1` | must stay stopped (`--restart=no`), port 8888 conflict |
| Rover WiFi | `192.168.0.1` — never touch | standing rule |
| Mac dongle | `AX88179A en7` = `192.168.0.2`, same LAN, different host | standing rule |

## 3. Firmware facts

| fact | value | status |
|------|-------|--------|
| `.shared` | `0xa8` @`0x38000000` both cores, magic `0x53485244` v2 | [UNVALIDATED] check `protocol-ok` line |
| CM4 `motor_ready` | set after `RARM_SetBank`+SPI test (`motor_ready_seq=1`) | [UNVALIDATED] |
| J6 hold (step2) | CM7 forces `shared[5]=0`, CM4 skips `J6_INDEX` | [UNVALIDATED] verify J6 physically still |
| No MPU / no DCache calls in BRINGUP trees | matches proven minimal | [UNVALIDATED] |
| STM does NOT auto-reconnect after agent restart | reset STM after any agent restart | observed pre-production |

## 4. Validation log (append per run)

```
date: 2026-09-05 (revalidation run)
step: step2 (BRINGUP/cm7_e2e + cm4_follow, poll-fix fw)
ping loss / rtt: 0%
serial highlights: seq=1 mdeg=9998 ack=0; seq=2 mdeg=-9998 ack=1; seq=3 mdeg=9998 ack=2
hardware observed: ARM MOVING (eyewitness, all 3 commands executed)
verdict: PASS — E2E J1–J5 motion via /joint_commands validated
```

```
date: 2026-09-05 ~14:0x UTC (session)
step: step2 (BRINGUP/cm7_e2e + cm4_follow, poll-fix fw)
ping loss / rtt: 0%, ~0.28ms
arp state: REACHABLE 00:80:e1:00:00:00
agent session: microk3 uros-agent, fresh restart + STM reset, client 0x75E3, entities created
serial highlights: joint-cmd-shared seq=1 mdeg=9998 ack=0; seq=2 mdeg=-9998 ack=1
joint_states readback: [0,0,0,0,-0.1744,0,...] then all ~0 after home cmds
hardware observed: ONE JOINT MOVED VIA TOPIC (eyewitness, >5min before log) — J5 tracked
  seq1 (+10) then seq2 (-10); J5 act readback matched commands; J1-J4 read exactly 0.0
  (unconfirmed whether slow/stuck — needs eyes during next run)
verdict: PARTIAL PASS — shared link bidirectional (cmd+ack+act), J5 motion confirmed,
  J1-J4 physical motion unconfirmed
```

Template for next runs:
```
date:
step:
ping loss / rtt:
arp state:
agent session (client_key / entities):
serial highlights:
joint_states readback:
hardware observed:
verdict: PASS / FAIL + reason
```

## 5. Issues log (append, never delete)

| date | symptom | cause (if known) | fix / workaround |
|------|---------|------------------|------------------|
| pre-prod | old-tree `CM7_e2e` no ARP + HardFault `CFSR 0x8200 BFAR 0xFFFFFF4C` @ scheduler start | suspect BSS growth (`537548`) + `standalone_path.c`; quarantined outside BRINGUP | use BRINGUP trees (BSS `410xxx`) |
| pre-prod | uplink dead (`joint_commands`→STM) while downlink OK | unknown; agent showed repeated `session re-established` | restart agent → reset STM (fresh XRCE) → republish |
| 2026-09-05 | uplink dead again after working (seq1/seq2 then silence), both QoS | **ROOT CAUSE: `rclc_executor_spin_some(...,5)` never delivers**; direct `rcl_take` poll works (proven on `cm7_echo_be`, then ported to all BRINGUP CM7 variants) | poll pattern in `StartJointCommandExecutorTask`; executor objects left allocated-but-unused |
| 2026-09-05 | uplink flaky across boots/sessions (works right after clean-slate, dies later) | suspect stale XRCE stream state on session re-establish (same client_key) | ritual: restart `uros-agent` → reset STM → publish promptly; validated working |
| 2026-09-05 | CM4 motion dead after repeated SW-reset flashes (J6 ±, `motor_ready=0`, stale SRAM `cfb`) although binaries verified | board/peripheral latch state, NOT firmware (same binary moved after full erase) | full erase (`-e all`) + reflash + reset revived J6 −3600°; when motion dies inexplicably, full-erase first before suspecting code |
| 2026-09-05 | all-motion dead, all drivers HiZ+UVLO live, PSU reads 24V | added auto-recovery (`Motor_RecoverDriver`: sticky 100 polls → `RARM_SetConfig` re-apply, no pin pulse; `drv_recovery_cnt` @CM4 `0x100001C4`, marker `0x80000000\|i` in `last_fault_code`) | recovery fires (cnt 16–18/joint) but faults re-latch instantly → HW condition SW can't clear; prime suspect blown 24V fuse between PSU and drivers (J1 high-current tests preceded outage); check fuse/continuity PSU→driver VS + e-stop loop |

## 6. micro-ROS agents inventory (FINAL: microk3 agent authoritative)

| agent | compose | mode | port | role |
|-------|---------|------|------|------|
| `uros-agent` | `microrosWs/microk3/docker-compose.yml`, `8888:8888/udp`, `udp4 --port 8888`, domain `${ROS_DOMAIN_ID:-0}` (=0) | bridge (binds all ifaces incl. `enx`) | 8888 | **FINAL authoritative — STM talks to this one** |
| `micro-ros-agent` (`jetson-orin-nx-micro-ros-agent-1`) | `production/jetson-orin-nx/docker-compose.hil*.yml`, `network_mode: host` | host | 8888 | legacy — **must be STOPPED** (port conflict + split-brain XRCE sessions) |

- `validate_net.sh` fails if production `micro-ros-agent` runs, if `uros-agent`
  is absent, or if agent count != 1.
- Switch-over on Jetson (one time):
  ```
  docker stop jetson-orin-nx-micro-ros-agent-1
  cd <microk3 checkout> && docker compose up -d uros-agent
  docker logs uros-agent --tail 3   # expect running... port 8888
  ```
  Bring only `uros-agent` up (not full compose) unless the dashboard is needed.
- All BRINGUP ros commands pin `ROS_DOMAIN_ID=0` (`common.sh ROS_ENV`,
  overridable via env) to match the agent. Both agents default domain 0,
  so this was never the uplink issue, but pinning removes the variable.
- `/joint_states` type conflict (`[sensor_msgs/JointState, std_msgs/Float32MultiArray]`)
  is expected: microk3 dashboard `ros_interface.py:100` subscribes `JointState`
  (`JSMsg`) while STM publishes `Float32MultiArray`. Always pass explicit type
  + `--qos-reliability best_effort` on `ros2 topic echo` (scripts already do).
  Dashboard arm feedback stays empty until types align (out of scope).
- `production` stack also runs `arm-bridge` (`/joint_states` Float32 in →
  `/hardware_level/states` JointState out, `/hardware_level/commands` in →
  `/joint_commands` out) — use it for ros2_control later, not for bringup.

## 7. Open questions for production

1. Does uplink work first-try on a fresh agent + fresh STM boot? (was dead last session)
2. J1–J5 physical direction/polarity correct for +10° commands?
3. `joint_states` rate/jitter acceptable at 20 ms on-demand publish?
4. J6 brake behavior in step 3 (no brake output on J6 — watch drift)?
