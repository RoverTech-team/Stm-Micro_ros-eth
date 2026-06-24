# Debug Log — H7 Port powerSTEP01 Daisy-Chain

## Fix #1 — SPI Baud Rate (applied)

**File:** `Core/Src/spi.c:14`

| Before | After |
|--------|-------|
| `SPI_BAUDRATEPRESCALER_32` | `SPI_BAUDRATEPRESCALER_256` |

**Why:** F4 (working) uses prescaler 256 (~351 kHz with APB2=90 MHz). H7 used 32 (~6.25 MHz with APB2=200 MHz). 18× faster on a long daisy-chain — signal integrity / reflections likely cause bit errors. Slowing to match F4 speed.

**Also changed in same file:** GPIO speed bumped to `VERY_HIGH` (see Fix C).

---

## Fix #2 — Mutex disables all interrupts (applied)

**Files:** `Core/Src/ps01_baremetal_os.c:18`

| Before | After |
|--------|-------|
| `__disable_irq()` / `__enable_irq()` | `__LDREXB`/`__STREXB` atomic spinlock with `__WFE()`/`__SEV()` |

**Why:** `__disable_irq()` kills SysTick (and all ISRs). `HAL_SPI_TransmitReceive` with `HAL_MAX_DELAY` may internally call `HAL_GetTick()` for timeouts — with SysTick dead, `HAL_GetTick()` returns stale value and the call may hang or timeout spuriously.

**Fix:** Exclusive-access spinlock — interrupts stay enabled, SysTick keeps ticking, `HAL_Delay` works.

---

## Fix #3 — Joint index order vs daisy-chain wiring (applied)

**File:** `Core/Src/drivers/RArm/rarm.h:10-15`

| Before | After |
|--------|-------|
| J1=0, J4=1, J6=2, J5=3, J3=4, J2=5 | J1=0, J2=1, J3=2, J4=3, J5=4, J6=5 |

**Why:** Index `active` selects which byte slot in the daisy-chain SPI frame carries the command (`tx[MOT_NUMBER-1-active]`). If the index order doesn't match the physical chip wiring, every register write goes to the wrong motor. Sequential order matches F4 (working) and the natural physical chain order.

**Note:** `main.c` uses designated initializers (`[J2_INDEX]` etc.) so the config values automatically map to the correct array slots — no reordering needed in the literal.

---

## Fix #4 — Extra BEMF register writes (applied)

**File:** `Core/Src/drivers/RArm/rarm.c:52-54`

| Before | After |
|--------|-------|
| `ps01SetParam_chain(ST_SLP, ...)` etc. | **Commented out** |

**Why:** F4 does NOT write these registers (they stay at power-on defaults = 0). H7 was writing user-defined values that may be incorrect for the actual motors, causing faults.

**Fix:** Lines 52-54 commented out. Registers stay at their reset defaults (0).

---

## Fix #5 — No SystemClock_Config on CM4 (applied)

**File:** `Core/Src/main.c`

| Before | After |
|--------|-------|
| No clock config (HSI 64 MHz default) | PLL from HSI: sysclk = 224 MHz |

**Why:** F4 calls `SystemClock_Config()` which sets up PLL → 180 MHz sysclk. H7 CM4 was running at HSI 64 MHz default with unknown APB prescalers, making SPI baud rate and TIM2 delay timing unpredictable.

**Fix:** Copy CM7's clock tree: HSI → PLLM=4, PLLN=28, PLLR=2 → sysclk = 224 MHz. APB1=112 MHz, APB2=112 MHz. `static void SystemClock_Config(void)` added in main.c, called right after `HAL_Init()`.

---

## Fix #6 — SPI FIFO threshold unconfigured (applied)

**File:** `Core/Src/spi.c` — added after `HAL_SPI_Init()`

| Before | After |
|--------|-------|
| Not set (default threshold) | `HAL_SPI_SetFifoThreshold(&hspi1, SPI_FIFO_THRESHOLD_01DATA)` |

**Why:** STM32H7 SPI has a 16-entry TX/RX FIFO. Without setting the threshold, the default may cause data underrun (TX) or overrun (RX) when the polling loop isn't fast enough. Setting to `01DATA` ensures every byte is transferred immediately (bare-metal compatibility).

---

## Fix A — TXC wait removed (was hanging after SPE=0) (applied)

**File:** `Core/Src/drivers/powerSTEP01/ps01.c:38,61`

| Before | After |
|--------|-------|
| `while(!__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_TXC)) {}` after `HAL_SPI_TransmitReceive` | Removed — CS raised immediately after HAL returns |

**Why:** H7 HAL `SPI_CloseTransfer` disables SPI (SPE=0) after EOT. Clearing SPE resets all SR status bits including TXC, so `while (!TXC)` hangs forever on the first transaction. The HAL already waited for EOT (transfer complete) before returning — no extra wait needed.

---

## Fix B — oc_threshold 20→40 A (applied)

**File:** `Core/Src/main.c` — `.oc_threshold = 40.0f` on all 6 joints

| Before | After |
|--------|-------|
| 20.0 A (reg≈12) | 40.0 A (reg≈25) (matches F4) |

**Why:** F4 uses 40 A on all joints. 20 A gives a lower trip threshold that might trigger false OCD (overcurrent detect) during motor switching, even at standstill due to inrush/spikes. 40 A matches the working F4 baseline.

---

## Fix C — SPI GPIO speed HIGH→VERY_HIGH (applied)

**File:** `Core/Src/spi.c:38,45`

| Before | After |
|--------|-------|
| `GPIO_SPEED_FREQ_HIGH` | `GPIO_SPEED_FREQ_VERY_HIGH` |

**Why:** F4 uses `GPIO_SPEED_FREQ_VERY_HIGH` for all SPI pins — SCK (PA5), MISO (PA6), MOSI (PA7 on F4, PB5 on H7). The faster slew rate ensures clean edges for the SPI clock and data signals, especially at the daisy-chain load.

---

## Fix D — SPI1 kernel clock source forced to pclk2 (applied)

**File:** `Core/Src/spi.c:25-26`

| Before | After |
|--------|-------|
| Not set (relies on reset value or CM7 config) | `RCC->D2CCIP1R = ... & ~RCC_D2CCIP1R_SPI123SEL` (forces `000` = pclk2) |

**Why:** SPI1 kernel clock is selected by `D2CCIP1R.SPI123SEL`. On H755 dual-core, CM7 may have changed this from `pclk2` (APB2) to `pll2_q_ck`, `pll3_q_ck`, or `hsi_ker_ck`. If the selected PLL is not enabled/stable, SPI1 has no valid clock and all transactions fail silently. Resetting SPI123SEL to `000` (= pclk2) guarantees SPI1 is clocked from APB2 (112 MHz).

---

## Fix #7 — Latched alarms never cleared; fault_alarm bits accumulate (applied)

**File:** `Core/Src/main.c` — `RR_PollFaults()` and button handler

**Problem 1:** `RR_PollFaults()` (called every 10ms) reads the powerSTEP01 STATUS register via `ps01GetStatus_chain()` but never clears the latched alarm bits. The STATUS register has latched fault flags — once a UVLO, stall, overcurrent, thermal, or command-error bit is set, it stays set forever. The only alarm-clearing path was the user-button handler, so the first transient fault (e.g., a UVLO glitch on the 24V rail during motor start, or a momentary stall) would permanently lock the joint out.

**Problem 2:** `SHARED_DATA->fault_alarm` used `|=` (OR-assignment) without ever clearing old bits, so stale fault flags accumulated across joints and cycles with no way to reset them.

**Fix (lines 412-428, 305-316):**
1. `RR_PollFaults()` now calls `ps01SetAlarms_chain()` with all alarm flags after detecting a fault, clearing the latched STATUS bits. Transient faults are reported once and then cleared; persistent faults are re-detected on the next 10ms poll.
2. Both `RR_PollFaults()` and the button handler now clear the joint's byte in `fault_alarm` before setting new bits (`&= ~(0xFF << shift)` before `|= alarm << shift`), so the shared-memory fault bitmap always reflects the live state.

---

## Fix #8 — Pre-config status poll stores stale power-up faults (applied)

**File:** `Core/Src/main.c` — removed initial status poll (was lines 243-252)

**Problem:** An initial `ps01GetStatus_chain()` loop ran for all 6 joints **before** `RARM_SetConfig()`. During power-up the STATUS register often has latched bits set (UVLO from the 24V rail ramping, CMD_ERROR from SPI noise, etc.). These were stored in `rr_faults[]` and `SHARED_DATA->fault_alarm`. Then `RARM_SetConfig()` would clear the hardware alarms (via `ps01SetAlarms_chain()` → ALARM_EN write), but the stale `rr_faults` entries persisted until the first `RR_PollFaults()` at the 10ms tick — by which time the wire-test mode had already seen `rr_faults[0] != 0` and skipped every joint.

The F446 reference never reads STATUS before configuration — it goes straight from reset-release to `RARM_SetConfig`.

**Fix:** Removed the pre-config status poll. `RARM_SetConfig()` is called first (clears hardware alarms via ALARM_EN write), then `rr_faults` and `fault_alarm` are explicitly zeroed. Real fault detection starts on the first 10ms `RR_PollFaults()` cycle.

---

## Fix #9 — Reset timing: 2300 ms hold → 1 ms hold + SPI flush (applied)

**File:** `Core/Src/main.c` — reset sequence (was lines 232-238)

| Before | After |
|--------|-------|
| DRV_RESET held LOW 2300 ms, then released, then waited 1100 ms total (100 + 1000) | Release briefly, SPI flush (`HAL_SPI_Transmit` 2 bytes of 0x00), assert 1 ms, release, 100 ms settling |

**Why:** The F446 reference uses a 1 ms reset pulse preceded by a 2-byte SPI transaction of `0x00` (dummy data). The H7 was holding reset for 2300 ms to wait for the 24 V rail, but this may let the charge-pump capacitors discharge. When reset is finally released, VCC ramps from 0 and can trigger UVLO-ADC. The F446's short pulse keeps the charge-pump primed, and the rail is already stable by this point (~2.5 s into boot due to earlier delays).

**Note (corrected in next fix):** The initial implementation mistakenly kept CS HIGH during the flush (GPIO init never toggles CS between mode-set and the first SPI call). The flush was a no-op. Fix #10 corrects this by asserting CS before the transmit.

---

---

## Fix #10 — Milli-degree → degree unit mismatch + dead SPI flush (applied)

### Fix 10a — `rarm.c:124`: `RARM_MoveMilliDegrees` passes milli-degrees to degree function

**File:** `Core/Src/drivers/RArm/rarm.c:124`

| Before | After |
|--------|-------|
| `ps01MoveDegrees_chain(dir, (uint32_t)delta);` | `if ((delta / 1000) > 0) ps01MoveDegrees_chain(dir, (uint16_t)(delta / 1000));` |

**Why:** `RARM_MoveMilliDegrees` receives `mdeg` in milli-degrees (e.g., 10 000 for a 10° move). It was passing `delta` directly to `ps01MoveDegrees_chain`, which expects **degrees** and internally calls `getStepsFromAngle(deg, ...)`. The function received 10 000 instead of 10, calculated for 10 000° (~27.7 rev), and commanded a step count **1111× too large**. The motor slammed into the mechanical stop, the powerSTEP01 detected a stall (STALL_A/STALL_B), and `RR_PollFaults` reported a persistent stall fault on every joint.

### Fix 10b — `main.c:235-241`: SPI flush was a no-op (CS was HIGH)

**File:** `Core/Src/main.c` — reset sequence

| Before | After |
|--------|-------|
| `DRV_RESET = HIGH` (release), then `HAL_SPI_Transmit` (CS still HIGH, ignored by driver), then assert/1ms/release/100ms | Assert CS, `HAL_SPI_Transmit` `N_JOINTS` bytes of 0x00, de-assert CS, then one clean 1 ms reset pulse, 100 ms settling |

**Why:** `DRV_CS` was initialized HIGH in `MX_GPIO_Init()` and never toggled before the flush call. `HAL_SPI_Transmit` does **not** manage CS — the caller must assert/de-assert it. Without CS LOW, the powerSTEP01 ignores all MOSI data. The flush was completely ineffective.

**Fix:** A `{ }` block asserts CS, transmits `N_JOINTS` bytes (full daisy-chain length, 6), then de-asserts CS. The VLA `flush[N_JOINTS]` is zero-filled to send `0x00` — NOP/SET_PARAM(ABS_POS=0) which has no lasting effect since the subsequent reset clears all state.

---

## Summary Table

| # | Issue | File | Status |
|---|-------|------|--------|
| 1 | SPI prescaler 32→256 | spi.c | **APPLIED** |
| 2 | `__disable_irq()` → atomic spinlock | ps01_baremetal_os.c | **APPLIED** |
| 3 | Joint index order sequential | rarm.h | **APPLIED** |
| 4 | Extra BEMF writes | rarm.c | **APPLIED** |
| 5 | No SystemClock_Config | main.c | **APPLIED** |
| 6 | SPI FIFO threshold | spi.c | **APPLIED** |
| A | TXC wait removed (was hanging after SPE=0) | ps01.c | **APPLIED** |
| B | oc_threshold 20→40 A | main.c | **APPLIED** |
| C | SPI GPIO speed HIGH→VERY_HIGH | spi.c | **APPLIED** |
| D | SPI1 kernel clock forced to pclk2 | spi.c | **APPLIED** |
| 7 | Latched alarms never cleared; fault_alarm bits accumulate | main.c | **APPLIED** |
| 8 | Pre-config status poll stores stale power-up faults | main.c | **APPLIED** |
| 9 | Reset timing: 2300 ms hold → 1 ms hold + SPI flush | main.c | **APPLIED** |
| 10a | `RARM_MoveMilliDegrees` passes milli-degrees to degree function | rarm.c | **APPLIED** |
| 10b | SPI flush was a no-op (CS was HIGH) | main.c | **APPLIED** |
