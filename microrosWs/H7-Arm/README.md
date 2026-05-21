# Motor Driver Integration (CM4)

Integration of powerSTEP01 drivers into the CM4 core for robotic arm motor control.

## What was done

### CM4 (bare-metal)

- Copied `powerSTEP01` and `RArm` drivers from the `network-perf-analysis` branch into `CM4/Core/Src/drivers/`
- Adapted includes for STM32H7 (they were originally for STM32F4)
- Created `ps01_baremetal_os.c`: bare-metal implementation of the `PS01_OS_t` interface (mutex via `__disable_irq`, semaphore via volatile DMA flag, delay via `HAL_Delay`)
- Rewrote CM4 `main.c`:
  - Added `HAL_Init()`, SPI1 + DMA init (correct order: DMA before SPI)
  - Driver init with `RARM_SetBank()` + `ps01Init()`
  - Motor control loop that reads commands from shared SRAM4 and calls `RARM_MoveDegrees()`
  - Reads actual positions back and writes them to SRAM4
  - Kept the ultrasonic sensor working alongside motor control
- Added placeholder chip-select pin (`DRV_CS_Pin` in `main.h`)

### CM7 (FreeRTOS + micro-ROS)

- Modified `ParseJointCommand()`: after parsing ROS messages, writes commanded positions to shared SRAM4 (with cache clean + DSB)
- Modified `BuildJointStatesJson()`: reads actual positions from CM4 (via SRAM4) instead of echoing back commanded values

### Shared Memory (SRAM4)

Expanded `shared_data.h` (on both cores) with:
- `joint_cmd_positions[6]` — CM7 writes, CM4 reads
- `joint_act_positions[6]` — CM4 writes, CM7 reads
- `joint_cmd_seq` / `joint_cmd_ack` — command sequence handshake
- `motor_ready` — flag set by CM4 once the driver is initialized

## Still missing (needs hardware)

- [ ] Actual chip-select pin (`DRV_CS_Pin` / `DRV_CS_GPIO_Port` in `CM4/Core/Inc/main.h`)
- [ ] Actual motor parameters (`RARM_SimpleConfig_t` — steps_rev, voltage, speeds, etc.)
- [ ] SPI pin confirmation
- [ ] Testing on the physical arm

## Changed/added files

```
CM4/Core/Src/main.c                              — rewritten (motor + sensor loop)
CM4/Core/Src/ps01_baremetal_os.c                  — new (bare-metal OS glue)
CM4/Core/Src/drivers/powerSTEP01/*                — copied + adapted for H7
CM4/Core/Src/drivers/RArm/*                       — copied from branch
CM4/Core/Inc/main.h                               — added DRV_CS defines
CM4/Core/Inc/shared_data.h                        — expanded with motor fields
CM7/Core/Inc/shared_data.h                        — expanded (same as CM4)
CM7/Core/Src/main.c                               — relay joint_commands + actual positions
```

