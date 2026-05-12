# powerSTEP01 — Bare-Metal Driver Integration Guide

The powerSTEP01 driver library has been designed with a clean OS-abstraction layer, allowing it to operate in any environment: RTOS-based (CMSIS-RTOS2, FreeRTOS, etc.) or bare-metal (no RTOS at all). The driver communicates with the powerSTEP01 stepper motor driver IC over SPI, using a daisy-chain topology that supports multiple motors on a single SPI bus.

All RTOS-specific calls for mutex management, semaphore synchronization, and timing have been factored out into a single injectable interface: **`PS01_OS_t`**. This guide explains how to integrate and use the driver in a bare-metal STM32 application where no real-time operating system is available.

---

## 1. Architecture

### 1.1 The `PS01_OS_t` Interface

The OS abstraction is defined in `ps01.h` and consists of a struct that holds two opaque handles (a mutex and a semaphore) and four function pointers. The driver never calls any RTOS API directly; instead, it calls through these function pointers. This design is sometimes called "dependency injection by function pointer" and is a well-established pattern in embedded systems for achieving portability without sacrificing runtime efficiency — each indirect call costs only one extra pointer dereference compared to a direct call.

```c
typedef struct {
    void *mutex;                           /* Opaque SPI-mutex handle        */
    void *semaphore;                       /* Opaque SPI-DMA semaphore handle*/
    void (*mutex_acquire)(void *mutex);    /* Block until mutex is owned     */
    void (*mutex_release)(void *mutex);    /* Release a previously held mutex*/
    void (*semaphore_acquire)(void *sem);  /* Block until semaphore available*/
    void (*delay_ms)(uint32_t ms);         /* Millisecond delay (may yield)  */
} PS01_OS_t;
```

| Member | Purpose | Called From |
|--------|---------|-------------|
| `mutex` | Opaque handle passed to `mutex_acquire`/`mutex_release`. In bare-metal this is typically `NULL`. | `_writebyte_chain`, `_readbyte_chain` |
| `semaphore` | Opaque handle passed to `semaphore_acquire`. In bare-metal this is typically `NULL`. | `_writebyte_chain`, `_readbyte_chain` |
| `mutex_acquire` | Enter a critical section or spin until the SPI bus is free. | Before every SPI transaction |
| `mutex_release` | Exit the critical section, releasing the SPI bus for other code. | After every SPI transaction |
| `semaphore_acquire` | Block or spin until the DMA transfer-complete interrupt fires. | After `HAL_SPI_Transmit_DMA` / `TransmitReceive_DMA` |
| `delay_ms` | Delay by the given number of milliseconds. Can be `HAL_Delay()` or a timer-based wait. | `_writebyte_chain`, `_readbyte_chain`, `ps01WaitBusy_chain` |

### 1.2 Driver Initialization Sequence

Before calling any motor-control API, the application must perform exactly two setup steps:

1. **`ps01SetBank()`** — tell the driver which `StepperBank_t` structure to use and how many motors are on the SPI chain.
2. **`ps01Init()`** — register the OS interface by passing a populated `PS01_OS_t` struct.

After these two calls, all other driver functions are available. The order matters because the low-level SPI helpers reference the OS interface internally; calling any SPI function before `ps01Init()` will dereference a NULL pointer and crash.

---

## 2. Bare-Metal Implementation

### 2.1 Global Variables

In a bare-metal system, the "mutex" and "semaphore" concepts collapse to simple `volatile` flags that are set by the main code and cleared by interrupt handlers. You will need two global volatile variables: one that indicates whether the SPI bus is currently in use (serving as the mutex), and one that indicates whether the DMA transfer has completed (serving as the semaphore).

These variables **must** be declared `volatile` because they are modified inside interrupt service routines and read in the main loop; without `volatile`, the compiler may optimize away the reads and your code will hang forever.

```c
/* ---- Bare-metal OS globals ---- */
static volatile uint8_t spi_bus_busy = 0;   /* 0 = free, 1 = in use */
static volatile uint8_t spi_dma_done = 0;   /* set by DMA ISR */
```

### 2.2 Implementing the OS Functions

Each function pointer in `PS01_OS_t` must be implemented to match the semantics expected by the driver. The following subsections explain the contract of each function and provide reference implementations suitable for bare-metal STM32 projects using the HAL library.

#### 2.2.1 `mutex_acquire`

In a bare-metal single-threaded environment, the "mutex" exists purely to protect against re-entrant SPI access from interrupt contexts. The simplest approach is to disable interrupts around the SPI transaction. If a more sophisticated approach is desired (for example, allowing higher-priority interrupts to run while blocking only SPI-related ones), you can disable only the SPI and DMA interrupts using `NVIC_DisableIRQ()`.

The function must not return until the caller has exclusive access to the SPI bus. Since there is no preemption in a single-threaded bare-metal main loop, this function will never actually block — it simply enters a critical section.

```c
static void bm_mutex_acquire(void *arg)
{
    (void)arg;
    __disable_irq();
    spi_bus_busy = 1;
}
```

#### 2.2.2 `mutex_release`

This function exits the critical section entered by `mutex_acquire`. It must re-enable interrupts and mark the bus as free. If you used NVIC-based selective masking in `mutex_acquire`, you must re-enable the corresponding IRQs here.

The pairing of acquire and release is guaranteed by the driver: every code path that acquires the mutex will release it before returning, including the error paths inside the SPI transfer functions.

```c
static void bm_mutex_release(void *arg)
{
    (void)arg;
    spi_bus_busy = 0;
    __enable_irq();
}
```

#### 2.2.3 `semaphore_acquire`

The semaphore represents the DMA transfer-complete signal. After calling `HAL_SPI_Transmit_DMA()` or `HAL_SPI_TransmitReceive_DMA()`, the driver calls `semaphore_acquire()` to wait until the DMA engine has finished moving all bytes.

In a CMSIS-RTOS2 environment this would block on an RTOS semaphore posted by the DMA-complete callback. In bare-metal, the simplest approach is to poll a flag that is set inside the HAL SPI transfer-complete callback. This is a busy-wait (spin loop), which is acceptable for short SPI transfers (typically a few microseconds for 1–4 bytes at several MHz).

If your system has other work to do during the wait, you can add a callback or yield mechanism inside the loop, but for most bare-metal applications the spin loop is perfectly adequate because the transfers are very short and deterministic.

```c
static void bm_semaphore_acquire(void *arg)
{
    (void)arg;
    /* Spin until the DMA transfer-complete ISR sets the flag */
    while (!spi_dma_done) {
        /* Optional: __WFI() to save power if interrupts are enabled */
    }
    spi_dma_done = 0;
}
```

#### 2.2.4 `delay_ms`

The driver uses millisecond delays in three places:

- A 1 ms inter-transaction guard delay in the SPI read/write functions
- A 1 ms wait inside the SPI-busy polling loop
- A 10 ms polling interval in `ps01WaitBusy_chain()`

In bare-metal, the STM32 HAL provides `HAL_Delay()`, which is based on the SysTick timer and works perfectly for this purpose. Note that `HAL_Delay()` requires SysTick to be configured and `HAL_IncTick()` to be called regularly (typically from the `SysTick_Handler`). If you are already using STM32CubeMX-generated code, this is set up by default.

```c
static void bm_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}
```

### 2.3 DMA Interrupt Handler

For the `semaphore_acquire` spin-loop to work, the DMA transfer-complete interrupt must set the `spi_dma_done` flag. In STM32 HAL projects, this is done inside the SPI DMA callback. The HAL calls `HAL_SPI_TxCpltCallback` or `HAL_SPI_TxRxCpltCallback` when the DMA transfer finishes.

The driver uses both `HAL_SPI_Transmit_DMA` and `HAL_SPI_TransmitReceive_DMA`, so **both** callbacks should be implemented.

```c
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        spi_dma_done = 1;
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        spi_dma_done = 1;
    }
}
```

> **Important:** If your application uses the SPI peripheral for other devices besides the powerSTEP01, you must check the SPI instance (`hspi->Instance`) in the callback to avoid incorrectly signaling completion for a different SPI bus. The example above checks for `SPI1`; adjust this to match the SPI instance used by the powerSTEP01 driver (configured by the `PS01_SPI_HANDLE` macro in `cfg_structs.h`).

---

## 3. Complete Integration Example

### 3.1 Full Implementation File (`ps01_baremetal_os.c`)

```c
/* ================================================================ */
/*  ps01_baremetal_os.c  -  Bare-metal OS glue for powerSTEP01     */
/* ================================================================ */

#include "ps01.h"
#include "stm32f4xx_hal.h"

/* ---- Global flags (set by ISR, read by OS functions) ---- */
static volatile uint8_t spi_dma_done = 0;

/* ---- OS function implementations ---- */

static void bm_mutex_acquire(void *arg)
{
    (void)arg;
    __disable_irq();
}

static void bm_mutex_release(void *arg)
{
    (void)arg;
    __enable_irq();
}

static void bm_semaphore_acquire(void *arg)
{
    (void)arg;
    while (!spi_dma_done) {
        /* Spin until DMA ISR fires */
    }
    spi_dma_done = 0;
}

static void bm_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ---- The OS interface instance ---- */
static const PS01_OS_t ps01_os = {
    .mutex             = NULL,            /* Not used; we use __disable_irq */
    .semaphore         = NULL,            /* Not used; we poll spi_dma_done */
    .mutex_acquire     = bm_mutex_acquire,
    .mutex_release     = bm_mutex_release,
    .semaphore_acquire = bm_semaphore_acquire,
    .delay_ms          = bm_delay_ms,
};

/* ---- DMA complete callbacks ---- */

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
        spi_dma_done = 1;
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
        spi_dma_done = 1;
}

/* ---- One-time init (call from main() before motor use) ---- */

void ps01_baremetal_init(StepperBank_t *bank, uint8_t n_motors)
{
    ps01SetBank(bank, n_motors);
    ps01Init(&ps01_os);
}
```

### 3.2 Usage in `main()`

```c
#include "ps01.h"
#include "cfg_structs.h"

/* Motor configuration */
static Stepper_t motors[2] = {
    {
        .steps_rev       = 200,
        .reduction_ratio = 1,
        .min_degs        = 0,
        .max_degs        = 360,
        .config.reg      = 0x0000,   /* Voltage mode defaults */
        .stepmode.reg    = 0x0000,   /* Full step */
        .kvals = { .acceleration = 20, .deceleration = 20,
                   .run = 25, .hold = 10 }
    },
    { /* second motor ... */ },
};

static StepperBank_t bank = {
    .motors = motors,
    .active = 0,
};

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();       /* DMA must init BEFORE SPI */
    MX_SPI1_Init();

    /* Initialize powerSTEP01 driver for bare metal */
    ps01_baremetal_init(&bank, 2);

    /* Select motor 0 and configure it */
    bank.active = 0;
    ps01SetConfig_chain();
    ps01SetStepMode_chain();
    ps01SetKVALs_chain();
    ps01SetMaxSpeed_chain(400);      /* 400 steps/s */
    ps01SetAcceleration_chain(200);  /* 200 steps/s² */

    /* Move motor 0 forward 180 degrees */
    ps01MoveDegrees_chain(1, 180);

    /* Wait for motion to complete */
    ps01WaitBusy_chain();

    while (1) {
        /* Main loop */
    }
}
```

---

## 4. API Quick Reference

All functions that end in `_chain` operate on the currently active motor as selected by `mot_bank->active`. Before calling any of these functions, you must have called `ps01SetBank()` and `ps01Init()` exactly once.

### 4.1 Initialization

| Function | Description |
|----------|-------------|
| `ps01SetBank(StepperBank_t *bank, uint8_t n)` | Set the motor bank pointer and the number of daisy-chained motors. |
| `ps01Init(const PS01_OS_t *os)` | Register the OS interface. Must be called after `ps01SetBank()`. |

### 4.2 Register Access

| Function | Description |
|----------|-------------|
| `ps01GetStatus_chain()` | Read the 16-bit STATUS register. |
| `ps01SetParam_chain(param, value)` | Write a parameter register (see `PARAM_t` enum). |
| `ps01GetParam_chain(param)` | Read a parameter register. |
| `ps01SetConfig_chain()` | Write the CONFIG register from motor config union. |
| `ps01SetStepMode_chain()` | Write the STEP_MODE register from motor stepmode union. |
| `ps01SetKVALs_chain()` | Write all four KVAL registers (ACC, DEC, RUN, HOLD). |

### 4.3 Speed and Threshold

| Function | Description |
|----------|-------------|
| `ps01SetMaxSpeed_chain(steps_s)` | Set maximum speed in steps/s (0–15610). |
| `ps01SetMinSpeed_chain(steps_s)` | Set minimum speed in steps/s. |
| `ps01SetAcceleration_chain(steps_s2)` | Set acceleration in steps/s². |
| `ps01SetDeceleration_chain(steps_s2)` | Set deceleration in steps/s². |
| `ps01SetFullStepSpeed_chain(steps_s)` | Set full-step speed threshold. |
| `ps01SetOcThreshold_chain(trip_amps)` | Set overcurrent threshold in Amps. |
| `ps01SetStallThreshold_chain(trip_amps)` | Set stall detection threshold in Amps. |
| `ps01SetAlarms_chain(alarm_bits)` | Enable alarm flags (see `ALARM_xxx` defines). |

### 4.4 Motion Commands

| Function | Description |
|----------|-------------|
| `ps01Run_chain(dir, steps_s)` | Run indefinitely at given speed. `dir`: 0=FWD, 1=REV. |
| `ps01StepClock_chain(dir)` | Switch to step-clock mode. |
| `ps01Move_chain(dir, n_steps)` | Move N microsteps in given direction. |
| `ps01MoveDegrees_chain(dir, deg)` | Move N degrees (accounts for step mode and gear ratio). |
| `ps01GoTo_chain(abs_pos)` | Go to absolute position (microsteps). |
| `ps01GoToDegrees_chain(abs_deg)` | Go to absolute position (degrees). |
| `ps01GoTo_DIR_chain(dir, abs_pos)` | Go to position forcing direction. |
| `ps01GoToDegrees_DIR_chain(dir, abs_deg)` | Go to degrees forcing direction. |
| `ps01GoUntil_chain(act, dir, steps_s)` | Run until switch event. `act`: 0=keep pos, 1=reset pos. |
| `ps01ReleaseSW_chain(act, dir)` | Release from switch. |

### 4.5 Stop and Home

| Function | Description |
|----------|-------------|
| `ps01GoHome_chain()` | Go to HOME position (ABS_POS = 0). |
| `ps01GoMark_chain()` | Go to MARK position. |
| `ps01ResetPos_chain()` | Reset ABS_POS to 0 without motion. |
| `ps01ResetDevice_chain()` | Full software reset of the powerSTEP01. |
| `ps01SoftStop_chain()` | Decelerate to stop. |
| `ps01HardStop_chain()` | Immediate stop (no deceleration). |
| `ps01SoftHiZ_chain()` | Decelerate then high-impedance (freewheel). |
| `ps01HardHiZ_chain()` | Immediate high-impedance. |
| `ps01WaitBusy_chain()` | Poll STATUS until BUSY bit clears. |
| `ps01GetPosition_chain()` | Read signed 22-bit ABS_POS register. |

---

## 5. Configuration Reference

### 5.1 `cfg_structs.h` Macros

| Macro | Default | Description |
|-------|---------|-------------|
| `PS01_SPI_HANDLE` | `hspi1` | HAL SPI handle used for all transfers. |
| `PS01_RDS_ON_HOT_OHM` | `0.0195f` | MOSFET Rds(on) at 75 °C (Ohms). |
| `PS01_OCD_STEP_V` | `0.03125f` | OCD register voltage step (31.25 mV). |
| `PS01_OCD_REG_MIN` | `0` | Minimum OCD register value. |
| `PS01_OCD_REG_MAX` | `31` | Maximum OCD register value. |

### 5.2 `Stepper_t` Configuration Fields

| Field | Type | Description |
|-------|------|-------------|
| `steps_rev` | `uint32_t` | Full steps per revolution of the motor (e.g. 200 for 1.8° motors). |
| `reduction_ratio` | `uint16_t` | Gear reduction multiplier (1 = direct drive). |
| `min_degs` | `int32_t` | Minimum position in degrees (application-defined, not used by driver). |
| `max_degs` | `int32_t` | Maximum position in degrees (application-defined, not used by driver). |
| `config` | `PS01ConfigVoltageMode_t` | CONFIG register union (oscillator, PWM freq, UVLO, etc.). |
| `stepmode` | `PS01StepMode_t` | STEP_MODE register union (microstep resolution, sync, CM/VM). |
| `kvals` | `PS01KVALs_t` | KVAL registers for voltage mode (hold, run, acceleration, deceleration). |

---

## 6. Porting Notes

### 6.1 Interrupt Safety

The bare-metal mutex implementation in Section 2 uses `__disable_irq()` / `__enable_irq()` to create critical sections. This is the simplest approach but has a side effect: **all interrupts are masked for the duration of the SPI transaction**, which typically takes 5–50 µs depending on the number of daisy-chained motors and the SPI clock speed.

If your application has hard real-time interrupts that cannot tolerate this latency, you have several alternatives:

- **Selective NVIC masking** — Disable only the SPI and DMA interrupts using `NVIC_DisableIRQ()` instead of the global `__disable_irq()`, which leaves other peripheral interrupts enabled.
- **Bare-metal spinlock** — Implement a proper spinlock that busy-waits on a `volatile` flag instead of disabling interrupts. This requires careful ordering to avoid race conditions.
- **BASEPRI masking** (Cortex-M3+) — Set `BASEPRI` to mask only interrupts at or below a configurable priority level, allowing higher-priority interrupts to fire even inside the critical section.

### 6.2 DMA Init Order

A very common STM32 pitfall is initializing the SPI peripheral before the DMA peripheral. The HAL requires that `MX_DMA_Init()` be called **before** `MX_SPI1_Init()` (or whichever SPI instance you use). If the order is reversed, DMA transfers will silently fail and the `spi_dma_done` flag will never be set, causing `semaphore_acquire()` to hang forever.

STM32CubeMX may not always enforce this ordering in its generated code, so it is worth checking your `main.c` initialization sequence. The recommended order is:

```
HAL_Init → SystemClock_Config → MX_GPIO_Init → MX_DMA_Init → MX_SPI1_Init → ps01_baremetal_init
```

### 6.3 Multiple SPI Devices

If your system shares the SPI bus between the powerSTEP01 and other peripherals (e.g., an ADC, display, or flash memory), the bare-metal mutex becomes more important. In this case, `__disable_irq()` is not sufficient because it does not prevent other parts of your main loop from starting an SPI transaction between the powerSTEP01 chip-select assertion and the DMA start.

You should implement a proper software mutex using a `volatile` flag:

- Set the flag before asserting CS
- Clear it after de-asserting CS
- Have all SPI-using code check the flag before starting a transaction

The `PS01_OS_t` `mutex` pointer can point to this flag, and `mutex_acquire` can spin-wait on it while `mutex_release` clears it. This ensures that only one device uses the SPI bus at a time without requiring an RTOS.

### 6.4 Switching to an RTOS Later

One of the major advantages of the `PS01_OS_t` abstraction is that migrating from bare-metal to an RTOS requires **zero changes to the driver source code**. You simply replace the bare-metal `PS01_OS_t` instance with one that wraps CMSIS-RTOS2 or FreeRTOS calls.

**CMSIS-RTOS2 example:**

```c
#include "cmsis_os2.h"

static void rtos_mutex_acquire(void *m)    { osMutexAcquire((osMutexId_t)m, osWaitForever); }
static void rtos_mutex_release(void *m)    { osMutexRelease((osMutexId_t)m); }
static void rtos_sem_acquire(void *s)      { osSemaphoreAcquire((osSemaphoreId_t)s, osWaitForever); }
static void rtos_delay(uint32_t ms)        { osDelay(ms); }

static const PS01_OS_t ps01_os = {
    .mutex             = (void *)MotorDriverMutexHandle,
    .semaphore         = (void *)driverSPISemaphoreHandle,
    .mutex_acquire     = rtos_mutex_acquire,
    .mutex_release     = rtos_mutex_release,
    .semaphore_acquire = rtos_sem_acquire,
    .delay_ms          = rtos_delay,
};
```

In both cases, the DMA callbacks must call the RTOS "give" or "release" function from ISR context (e.g., `xSemaphoreGiveFromISR` or `osSemaphoreRelease`) instead of setting a volatile flag.

---

## 7. Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Driver hangs at first SPI call | `ps01Init()` was never called, or `ps01_os` is NULL | Call `ps01SetBank()` then `ps01Init(&ps01_os)` before any motor API. |
| Driver hangs in `semaphore_acquire` | DMA callback not firing or `spi_dma_done` never set | Check DMA init order (`MX_DMA_Init` before `MX_SPI1_Init`). Verify SPI1 callback matches `PS01_SPI_HANDLE`. |
| SPI data is corrupted | SPI clock polarity/phase mismatch | PowerSTEP01 requires CPOL=0, CPHA=1 (SPI Mode 1). Check HAL SPI init. |
| Motor does not move after command | CONFIG or STEP_MODE not written to chip | Call `ps01SetConfig_chain()` and `ps01SetStepMode_chain()` after `ps01Init()`. |
| Hard fault in `_writebyte_chain` | `MOT_NUMBER` is 0 or `mot_bank` is NULL | Call `ps01SetBank()` with valid bank and `n_motors > 0`. |
| OCD triggers immediately | OCD threshold too low | Increase `trip_amps` in `ps01SetOcThreshold_chain()`. Min register value (0) = 312.5 mA. |
