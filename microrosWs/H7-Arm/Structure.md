# STM32H755 – 6-DOF Robotic Arm Firmware Architecture

## Overview

The STM32H755 is a dual-core microcontroller (Cortex-M7 + Cortex-M4) running in Asymmetric  
Multiprocessing (AMP) mode. The two cores have strictly separated responsibilities:

| Core | Clock | OS | Role |
| :--- | :--- | :--- | :--- |
| **Cortex-M7** | 480 MHz | FreeRTOS | Network gateway, micro-ROS, wrist kinematics |
| **Cortex-M4** | 240 MHz | Bare Metal | Hard real-time cascade PIDs, motor control |

Communication between cores is handled exclusively through a **shared SRAM block** in the D2  
domain (`SRAM3`, base address `0x30040000`), protected by STM32 Hardware Semaphores (HSEM).

---

## Shared Memory Layout

```c
// Mapped to SRAM3 in both M7 and M4 linker scripts
// Section: .shared_ram  @0x30040000
// Must be placed in non-cacheable MPU region on the M7 side
typedef struct {
    float target_position[6];   // Written by M7 (decoupled motor targets)
    float target_velocity[6];   // Written by M7 (decoupled motor velocity targets)
    float actual_position[6];   // Written by M4 (encoder readings)
    float actual_velocity[6];   // Written by M4 (derived from encoders)
    uint32_t m4_status;         // M4 heartbeat / fault flags
} SharedMemory_t;
```

> ⚠️ The M7 must configure an MPU region over this struct as Non-Cacheable  
> (`TEX=1, C=0, B=0`) to prevent D-Cache coherency issues with the Ethernet DMA  
> and the M4 AXI bus access. The M4 accesses shared SRAM directly with no caching.

---

## Cortex-M7 – FreeRTOS Task Map

### Task 1: micro-ROS Executor Task

| Property | Value |
| :--- | :--- |
| **Function** | Subscribes to `/joint_cmds`, publishes to `/joint_states` |
| **Environment** | FreeRTOS Task |
| **Timing** | Event-driven (`rclc_executor_spin_some()` with `vTaskDelay(1)`) |
| **Priority** | High (FreeRTOS priority 4/5) |
| **Stack Size** | ~8 KB (XRCE-DDS serialization overhead) |
| **Library** | `micro_ros_stm32cubemx_utils`, `rclc`, `rcl`, `rmw_microxrcedds` |
| **Transport** | UDP over Ethernet via LwIP (`LWIP_UDP` enabled) |

**Responsibilities:**
- Receives `Float32MultiArray` joint command messages from `ros2_control` on the PC
- Calls `decouple_wrist()` inline to compute Motor 5 and Motor 6 targets from J5/J6 inputs
- Writes all 6 motor targets (position + velocity) to the shared memory block
- Reads actual states from shared memory and publishes `/joint_states` back to the PC

---

### Task 2: LwIP Network Stack Task

| Property | Value |
| :--- | :--- |
| **Function** | Handles UDP/IP Ethernet communication |
| **Environment** | FreeRTOS Task (spawned internally by LwIP) |
| **Timing** | RTOS Scheduler driven, interrupt-triggered by Ethernet DMA |
| **Priority** | Highest FreeRTOS priority (must preempt micro-ROS task on packet arrival) |
| **Library** | `lwip` (STM32CubeH7 middleware), `ethernetif.c` HAL driver |
| **Hardware** | Ethernet MAC peripheral + external PHY (e.g., LAN8742) |

**Critical Configuration:**
- Ethernet DMA Rx/Tx descriptors and packet buffers **must** be in non-cacheable MPU region
- MPU attribute: `ARM_MPU_RASR(TEX=1, C=0, B=0)` (Non-cacheable, Non-bufferable)
- Buffer alignment: **32-byte aligned** (Cortex-M7 cache line requirement)

---

### Task 3: Wrist Decoupling (Inline Function — Not a Separate Task)

| Property | Value |
| :--- | :--- |
| **Function** | Converts J5/J6 joint targets → Motor 5/6 targets and vice versa |
| **Environment** | Pure C `static inline` function called within Task 1 |
| **Timing** | Executes synchronously inside the micro-ROS subscriber callback |
| **Library** | None — raw FPU arithmetic using GCC hardware float ABI |

```c
// wrist_kinematics.h
// Gear ratios exposed as micro-ROS parameters for runtime configuration
#define GEAR_RATIO_PITCH 10.0f
#define GEAR_RATIO_ROLL  10.0f

// Inverse Transmission: Joint targets -> Motor targets (M7 writes to shared memory)
static inline void decouple_wrist(float j5, float j6, float *m5, float *m6) {
    *m5 = (j5 * GEAR_RATIO_PITCH) + (j6 * GEAR_RATIO_ROLL);
    *m6 = (j5 * GEAR_RATIO_PITCH) - (j6 * GEAR_RATIO_ROLL);
}

// Forward Transmission: Motor states -> Joint states (M7 reads from shared memory)
static inline void couple_wrist(float m5, float m6, float *j5, float *j6) {
    *j5 = (m5 + m6) / (2.0f * GEAR_RATIO_PITCH);
    *j6 = (m5 - m6) / (2.0f * GEAR_RATIO_ROLL);
}
```

> Gear ratios can be fetched at boot via micro-ROS Parameter Server  
> to avoid reflashing for mechanical reconfiguration.

---

### Task 4: Heartbeat and Load Monitor Task

| Property | Value |
| :--- | :--- |
| **Function** | Blinks LED, reports M7/M4 CPU load, monitors M4 watchdog via shared memory |
| **Environment** | FreeRTOS Task |
| **Timing** | `vTaskDelay(1000)` — 1 Hz |
| **Priority** | Lowest FreeRTOS priority |
| **Library** | `FreeRTOS` (`uxTaskGetSystemState()`), HAL GPIO |

---

### Task 5: Full Logging Task

| Property | Value |
| :--- | :--- |
| **Function** | Streams joint states, PID outputs, and fault flags over UART for debug |
| **Environment** | FreeRTOS Task |
| **Timing** | `vTaskDelay(10)` — 100 Hz |
| **Priority** | Low FreeRTOS priority |
| **Library** | HAL UART with DMA (`HAL_UART_Transmit_DMA`) |

---

## Cortex-M4 – Bare Metal Task Map

> The M4 runs **no operating system**. All tasks are either hardware timer interrupts  
> or background superloop operations in `main()`. FreeRTOS is not used on the M4 —  
> this guarantees zero scheduler jitter on the 10 kHz control loop.

---

### Task 6: Cascade PID Control Loop ⚡ (CORE TASK)

| Property | Value |
| :--- | :--- |
| **Function** | Executes 6 parallel cascade position → velocity PID loops |
| **Environment** | **Hardware Timer Interrupt** (`TIM3_IRQHandler`) |
| **Timer** | **TIM3** (APB1 domain, M4 exclusive) |
| **Frequency** | **10 kHz** (100 µs period) |
| **NVIC Priority** | 0 (highest possible, never blocked) |
| **Library** | `CMSIS-DSP` (`arm_pid_f32`, `arm_pid_instance_f32`) |
| **Memory** | PID state structs pinned to SRAM4 (`__attribute__((section(".sram4")))`) |

**Controller Structure per Joint:**

```
θ target → [P Controller] → V target → [PI Controller] → Motor Effort → PWM
               outer loop                  inner loop
```

| Loop | Controller | Gains Active | CMSIS-DSP Call |
| :--- | :--- | :--- | :--- |
| Outer (Position) | P only | Kp only (`Ki=0, Kd=0`) | `arm_pid_f32(&pid_pos[i], pos_error)` |
| Inner (Velocity) | PI only | Kp + Ki (`Kd=0`) | `arm_pid_f32(&pid_vel[i], vel_error)` |

**Estimated Execution Time:** ~2 µs for all 6 joints → **2% CPU utilization** at 10 kHz

```c
void TIM3_IRQHandler(void) {
    if (__HAL_TIM_GET_FLAG(&htim3, TIM_FLAG_UPDATE) != RESET) {
        __HAL_TIM_CLEAR_IT(&htim3, TIM_IT_UPDATE);

        for (int i = 0; i < 6; i++) {
            // Outer loop: position
            float pos_error = shared_mem->target_position[i] - shared_mem->actual_position[i];
            float target_vel = arm_pid_f32(&pid_pos[i], pos_error);
            if (target_vel >  MAX_VELOCITY) target_vel =  MAX_VELOCITY;
            if (target_vel < -MAX_VELOCITY) target_vel = -MAX_VELOCITY;

            // Inner loop: velocity
            float vel_error = target_vel - shared_mem->actual_velocity[i];
            float effort    = arm_pid_f32(&pid_vel[i], vel_error);
            if (effort >  MAX_EFFORT) effort =  MAX_EFFORT;
            if (effort < -MAX_EFFORT) effort = -MAX_EFFORT;

            set_motor_pwm(i, effort);
        }
    }
}
```

---

### Task 7: Encoder Reading

| Property | Value |
| :--- | :--- |
| **Function** | Reads quadrature encoder counts, derives velocity by delta-position / delta-time |
| **Environment** | Executed at the **start** of `TIM3_IRQHandler` before PID calculations |
| **Timers** | TIM2, TIM4, TIM5 (32-bit), TIM1, TIM8, TIM12 (see table below) |
| **Library** | HAL TIM Encoder Mode (`TIM_ENCODERMODE_TI12`) |

---

### Task 8: PWM Motor Output

| Property | Value |
| :--- | :--- |
| **Function** | Drives 6 motor H-bridge drivers via complementary PWM |
| **Environment** | Updated at the **end** of `TIM3_IRQHandler` after PID calculations |
| **Timers** | **TIM1** (Motors 1 & 2), **TIM8** (Motors 3 & 4), **TIM15/16** (Motors 5 & 6) |
| **Library** | HAL TIM PWM (`__HAL_TIM_SET_COMPARE`) |
| **Features** | TIM1 and TIM8 support complementary outputs + hardware dead-time insertion |
| **Frequency** | 20–50 kHz switching frequency |

> TIM1 and TIM8 are the two **Advanced Control Timers** on the STM32H755.  
> They are the correct choice for H-bridge PWM as they natively support  
> complementary outputs (CHx / CHxN) and dead-time without external logic.  
> HRTIM is not used — it is designed for power electronics (DC-DC converters,  
> inverters) and is unnecessarily complex for robotic arm motor driving.

---

### Task 9: Shared Memory State Write

| Property | Value |
| :--- | :--- |
| **Function** | Writes actual encoder positions and velocities to shared SRAM for the M7 |
| **Environment** | Executed at the end of `TIM3_IRQHandler`, after PWM is updated |
| **Timing** | Every 10 kHz tick (100 µs) |
| **Synchronization** | No mutex needed — M4 writes actual states, M7 only reads them; M7 writes targets, M4 only reads them. No simultaneous read-write on the same field. |

---

### Task 10: Fault Detection and Safety Superloop

| Property | Value |
| :--- | :--- |
| **Function** | Monitors overcurrent (ADC), encoder timeouts, and M4 watchdog |
| **Environment** | `while(1)` background superloop in `main.c` |
| **Timing** | Continuous background — interrupted by TIM3 every 100 µs |
| **Library** | HAL ADC (current sensing), HAL GPIO (fault pins from motor drivers) |

---

## Timer Allocation Summary

| Timer | Type | Core | Mode | Frequency | Assigned To |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **SysTick** | Core | M7 | FreeRTOS Tick | 1 kHz | FreeRTOS Scheduler |
| **TIM1** | Advanced | M4 | Encoder + PWM Ch1/2 | Hardware / 20–50 kHz | Motor 1 Encoder + Motors 1 & 2 PWM |
| **TIM2** | General 32-bit | M4 | Encoder Interface | Hardware | Motor 2 Encoder |
| **TIM3** | General 16-bit | M4 | **Update Interrupt** | **10 kHz** | **Cascade PID ISR trigger** |
| **TIM4** | General 16-bit | M4 | Encoder Interface | Hardware | Motor 3 Encoder |
| **TIM5** | General 32-bit | M4 | Encoder Interface | Hardware | Motor 4 Encoder |
| **TIM6** | Basic | M7 | HAL Timebase | 1 kHz | `uwTick` for FreeRTOS HAL |
| **TIM7** | Basic | M7 | Reserved | — | Future M7 use / LPTIM fallback |
| **TIM8** | Advanced | M4 | Encoder + PWM Ch1/2 | Hardware / 20–50 kHz | Motor 5 Encoder + Motors 3 & 4 PWM |
| **TIM12** | General 16-bit | M4 | Encoder Interface | Hardware | Motor 6 Encoder |
| **TIM15** | General 16-bit | M4 | PWM Output | 20–50 kHz | Motor 5 PWM |
| **TIM16** | General 16-bit | M4 | PWM Output | 20–50 kHz | Motor 6 PWM |
| **TIM17** | General 16-bit | M4 | Reserved | — | Future (current sensing trigger / ADC sync) |
| **HRTIM** | High-Res | — | **Not Used** | — | Designed for power electronics. Not suitable for robotic arm motor control. |
| **RTC** | RTC | — | **Not Used** | — | Calendar/low-power wakeup only. Not suitable for control-loop timing. |

---

## Library Summary

| Library | Core | Purpose |
| :--- | :--- | :--- |
| `rclc` / `rcl` | M7 | micro-ROS client API (Pure C) |
| `rmw_microxrcedds` | M7 | XRCE-DDS transport layer for micro-ROS |
| `lwip` | M7 | UDP/IP networking stack over Ethernet |
| `FreeRTOS` | M7 | Task scheduling and timing |
| `STM32H7 HAL` | M7 + M4 | Peripheral drivers (Ethernet, UART, TIM, GPIO, ADC) |
| `CMSIS-DSP` | M4 | `arm_pid_f32` — optimized cascade PID |
| `CMSIS-Core` | M7 + M4 | MPU configuration, D-Cache control, HSEM |

---

## Compiler Flags

### M7 Sub-project
```
-mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 -O2
-DARM_MATH_CM7 -D__FPU_PRESENT=1
```

### M4 Sub-project
```
-mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O2
-DARM_MATH_CM4 -D__FPU_PRESENT=1
```

> Both sub-projects use `float32_t` (`float`) exclusively.  
> The M4 only has a single-precision FPU — using `double` forces software emulation  
> and will destroy 10 kHz loop timing.

---

## Memory Map Overview

| Region | Address | Size | Core | Used By |
| :--- | :--- | :--- | :--- | :--- |
| DTCM | `0x20000000` | 128 KB | M7 | FreeRTOS task stacks, wrist kinematics inline functions |
| AXI SRAM | `0x24000000` | 512 KB | M7 | LwIP packet buffers (non-cacheable MPU region) |
| SRAM1/2 | `0x30000000` | 256 KB | M7 | micro-ROS heap, DDS entity buffers |
| **SRAM3 (Shared)** | `0x30040000` | 32 KB | **M7 + M4** | **`SharedMemory_t` — IPC between cores** |
| SRAM4 | `0x38000000` | 64 KB | M4 | CMSIS-DSP `arm_pid_instance_f32` state arrays |

---

## Notes and Design Decisions

- **HRTIM excluded:** HRTIM is a power-electronics timer designed for sub-nanosecond PWM  
  in DC-DC converters and inverters. TIM1 and TIM8 (Advanced Control Timers) provide  
  all the features needed for robotic arm H-bridge driving: complementary outputs,  
  dead-time insertion, and break inputs for fault protection.

- **RTC excluded from control path:** The RTC runs from a 32.768 kHz crystal and is  
  suitable only for calendar timekeeping and low-power wakeup. It cannot generate  
  deterministic high-frequency interrupts.

- **No FreeRTOS on M4:** The M4 runs bare metal to guarantee zero scheduler jitter on  
  the 10 kHz TIM3 interrupt. FreeRTOS task switching latency (~200–500 cycles) would  
  introduce timing instability in the cascade PID derivative and integral terms.

- **No mutex on shared memory:** The shared memory struct is partitioned by access direction.  
  The M4 writes only `actual_*` fields; the M7 writes only `target_*` fields. Since  
  float writes on ARM Cortex are atomic at the bus level for aligned 32-bit values,  
  no semaphore is required for single-producer / single-consumer fields.

---

*Document version: 1.1 – STM32H755 6-DOF Arm Firmware Architecture*  
*Updated: timer allocation revised — HRTIM removed, TIM1/TIM8 confirmed as PWM generators,*  
*RTC excluded from control path, TIM12 added to resolve 6th encoder.*
