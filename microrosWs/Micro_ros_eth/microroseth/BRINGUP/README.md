# BRINGUP — STM32H755 arm bringup (self-contained)

One directory per firmware job. Each subdir has its **own Makefile + `build/`**,
so rollback = rebuild + flash the other variant. No shared build state.

```
BRINGUP/
  Makefile         root: make all|clean|manifest|step1|step2|step3|test-net
  STEPS.md         gated runbook: flash → validate → next
  BUILD_MANIFEST.md sizes + sha256 (regen: make manifest)
  cm7_echo/      CM7 minimal: heartbeat + joint_commands→joint_states ECHO (proven)
  cm7_e2e/       CM7 E2E: joint_commands → shared SRAM4 (J1-J5, J6 forced 0)
  cm7_e2e_full/  CM7 E2E full: shared incl. J6 (ATTENDED, step 3 only)
  cm4_follow/    CM4: follows shared J1-J5, J6 skipped/held
  cm4_full/      CM4: follows shared incl. J6 (ATTENDED, step 3 only)
  scripts/       flash + validate + test scripts (step1/2/3, validate_net,
                 test_net, test_joints, build_all, gen_manifest, common)
```

Steps: **step1** = `cm7_echo` + `cm4_follow` (net/echo, no motion) →
**step2** = `cm7_e2e` + `cm4_follow` (J1–J5 ±10°, J6 = 0) →
**step3** = `cm7_e2e_full` + `cm4_full` (all 6, attended, confirm `YES`).

## Network

- Rover/Jetson WiFi: `192.168.0.1` (`enP1p1s0`) — untouched.
- STM **on the rover LAN**: `192.168.0.5`, GW `192.168.0.1` (was `192.170.0.2`
  on the retired direct dongle link). Agent UDP `8888` served by microk3
  `uros-agent` (authoritative; production agent stopped).
  IP is baked at build time: `MICROROS_DEVICE_IP_*` / `MICROROS_AGENT_IP`
  in each CM7 `Makefile` (defaults now `192.168.0.5` / `192.168.0.1`).
- MAC of the STM: `00:80:e1:00:00:00`.
- Mac `AX88179A en7` is `192.168.0.2` (same LAN, separate host — no `.5` conflict).

## Variants

| dir | TARGET | sources from | behavior |
|-----|--------|--------------|----------|
| `cm7_echo` | `MicroRosEth_CM7_echo` `158724/1048/410388` | `microroseth/CM7_arm` copy | ping OK, `joint-cmd-recv` echo, no shared writes |
| `cm7_e2e` | `MicroRosEth_CM7_e2e` `159452/1048/410564` `.shared 0xa8 @0x38000000` | `microroseth/CM7_arm_e2e` copy | magic-gated (`0x53485244/2`) shared write J1-J5, J6=0, on-demand publish from shared |
| `cm7_e2e_full` | `MicroRosEth_CM7_e2e_full` `159452/1048/410564` `.shared` | `cm7_e2e` minus J6 skip | shared write incl. J6 (step 3 only) |
| `cm4_follow` | `MicroRosEth_CM4_follow` `14444/388/2092` `.shared 0xa8 @0x38000000` | `H7arm/CM4` copy, `-DWIRE_TEST=0` | `Motor_ProcessCommands`, `J6` skipped, brakes J2/J3 |
| `cm4_full` | `MicroRosEth_CM4_full` `14440/388/2092` `.shared` | `cm4_follow` minus J6 skip | follows incl. J6 (step 3 only) |

> NOTE 2026-10-05: orphan `cm7_echo_be` (undocumented 6th variant, `rcl_take` poll
> test fw) removed from working tree to dedup. Restore from
> `backups/BRINGUP_backup_20261005_163218.tar.gz` or
> `backups/BRINGUP_cm7_echo_be_orphan_20261005_163218.tar.gz`.
> `build/` dirs cleaned (regenerable via `make all`); sizes in table below are
> pre-cleanup — see `BUILD_MANIFEST.md` (regen: `make manifest`).

`cm4_follow` references vendor trees relatively
(`../../../../H7-Arm/H7arm/{Drivers,Middlewares,Common}`); CM7 variants are
fully self-contained except `../../micro_ros_stm32cubemx_utils` (stable).

> NOTE 2026-10-05 (single-source): all 3 CM7 `Core/Src/main.c` are now
> byte-identical — behavior selected by Makefile `-DBRINGUP_E2E=0/1`
> `-DBRINGUP_J6_HOLD=0/1` (same pattern as existing `-DWIRE_TEST`).
> Both CM4 `main.c` likewise identical, J6 hold via `-DBRINGUP_J6_HOLD`.
> Safe defaults (`E2E=0`, `J6_HOLD=1` = echo/hold, no motion) apply if flags
> are ever absent. Verified: rebuild produces bit-identical sizes
> (`146524/147244/147228` + `14644/14640`, see `BUILD_MANIFEST.md`).
> `utils` canonical: `microroseth/micro_ros_stm32cubemx_utils` is newest
> (has `w5500_transport.c` + `MICROROS_ASSUME_CFLAGS_YES`); the 3 other
> copies (`TestArm/`, `sensor_fusion/`, `H7arm/`) are legacy — do not
> symlink/merge blindly, transport + libgen scripts differ.

## Build (offline-safe)

```bash
make -C cm7_echo -j4
make -C cm7_e2e -j4
make -C cm4_follow -j4
```

## Flash (needs ST-Link)

```bash
./scripts/flash_echo.sh   # baseline/rollback: echo + follow
./scripts/flash_e2e.sh    # E2E: shared J1-J5 + follow
```

CM7 @`0x08000000`, CM4 @`0x08100000`. Both scripts mass-erase first.
`CLI` env overrides programmer path.

## Rollback (instant)

E2E misbehaves → `./scripts/flash_echo.sh` (back to proven ping+echo in ~1 min).

## Verify (needs Jetson `192.168.0.1`)

```bash
./scripts/test_net.sh        # ping 192.168.0.5 from Jetson, ARP 00:80:e1:00:00:00
./scripts/test_joints.sh     # J1-J5 +-10°, then joint_states once
```

Serial (Mac): `/dev/tty.usbmodem1203` `115200`.
- echo: `CM7: joint-cmd-recv size=18 seq0=0.17` + `joint-states-echo`
- e2e: `CM7: protocol-ok magic=0x53485244 version=2` then
  `CM7: joint-cmd-shared seq=N pos0=0.17 mdeg=10000 ack=...`

## Known issues

- `H7arm/Makefile/CM7_e2e` (old tree) HardFaults at scheduler start
  (`CFSR 0x8200` precise bus, `BFAR 0xFFFFFF4C`) and never answers ARP.
  Suspect: BSS growth (`537548` vs `410xxx`: heap `194560` vs `131072`)
  + `standalone_path.c`. The `BRINGUP/cm7_e2e` rebuild from the minimal
  tree avoids it (BSS `410564`).
- Uplink (`/joint_commands`, `/time_sync_request` → STM) was dead in last
  session while downlink (`/heartbeat`) worked; agent showed repeated
  `session re-established`. If it persists: restart agent container, reset
  STM (fresh XRCE session), then publish.
- STM does not auto-reconnect if the agent restarts (`publish-failed`
  + `runtime-fault=publish` loop) — reset STM after agent restart.
