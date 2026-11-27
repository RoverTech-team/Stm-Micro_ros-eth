# STEPS — flash → validate → next (production runbook)

Run in order. Each step is gated: do not advance on FAIL.
Rover `192.168.0.1` WiFi is never touched. All Jetson commands go over
`RoverTech@192.168.0.1`; the STM sits on the rover LAN at `192.168.0.5`
(GW `192.168.0.1`), agent UDP `8888` on the Jetson.

```
make step1  →  validate  →  make step2  →  validate  →  make step3 (attended)
```

## STEP 1 — baseline: net + echo (no motion)

- Flash: `cm7_echo` (`MicroRosEth_CM7_echo` @`0x08000000`) + `cm4_follow` (@`0x08100000`).
- Command: `make step1` (or `./scripts/step1.sh`).
- Expect:
  1. `ping 192.168.0.5` from Jetson: 0% loss, `~0.5ms`.
  2. `ip neigh show 192.168.0.5` → `REACHABLE`/`STALE` (never `FAILED`), MAC `00:80:e1:00:00:00`.
  3. `ros2 topic list` shows `/heartbeat /joint_states /joint_commands`.
  4. `ros2 topic echo /heartbeat` → `data: N` increasing.
  5. Serial `/dev/tty.usbmodem* 115200` → `CM7: joint-cmd-recv size=18`.
- On FAIL: check the Jetson is on `192.168.0.0/24`, the STM is cabled/powered
  (link LED), the agent container is up, then re-run. Do NOT go to step 2.
- Authoritative agent is microk3 `uros-agent` (bridge, 8888, domain 0);
  production `micro-ros-agent` must be stopped — `validate_net.sh`
  enforces single-agent. Switch-over + details: `additional_context.md §6`.

## STEP 2 — shared E2E, J1–J5 (J6 held at 0)

- Flash: `cm7_e2e` + `cm4_follow`. Prereq: step 1 passes.
- Command: `make step2` (or `./scripts/step2.sh`).
- Expect:
  1. `validate_net.sh` passes again after flash.
  2. Serial → `CM7: protocol-ok magic=0x53485244 version=2` (CM4 alive).
  3. After `+10°` pub → `CM7: joint-cmd-shared seq=N pos0=0.17 mdeg=10000`.
  4. After `-10°` pub → same with `pos0=-0.17`.
  5. `ros2 topic echo /joint_states` → J1–J5 track `±0.17`, index 5 stays `0.0`.
  6. **Hardware**: J1–J5 move ±10°, J6 still.
- On FAIL:
  - No `protocol-ok` → CM4 not running (reflash `cm4_follow`, check `motor_ready`).
  - No `joint-cmd-shared` → uplink dead: restart agent container, reset STM
    (fresh XRCE session), re-run pub. STM does not auto-reconnect after
    agent restart (`publish-failed` loop) — always reset STM after agent restart.
  - Ping dead but step 1 passed → re-run `make step1` (rollback, ~1 min),
    then retry step 2 once. If still dead, stop: quarantine to old-tree
    HardFault analysis (`H7arm/Makefile/CM7_e2e`, `CFSR 0x8200`).

## STEP 3 — FULL arm incl. J6 (ATTENDED motion)

- Flash: `cm7_e2e_full` + `cm4_full` (J6 skips removed on both cores).
- Command: `make step3` — requires typing `YES` (arm will move J6 ±10°).
- Expect: same as step 2, plus index 5 tracks `±0.17` and J6 physically moves.
- Rollback any time: `make step1`.

## Quick reference

| cmd | does |
|-----|------|
| `make all` | build all 5 variants |
| `make manifest` | rebuild `BUILD_MANIFEST.md` |
| `make clean` | clean all `build/` |
| `make step1/2/3` | flash + validate that step |
| `make test-net` | net check only |
| `./scripts/test_joints.sh [once\|+10\|-10\|both]` | manual J1–J5 pubs + readback |

Serial (Mac): first `/dev/tty.usbmodem*` @`115200` (`SERIAL_PORT` overrides).
