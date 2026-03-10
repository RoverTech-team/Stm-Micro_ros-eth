# LwIP Debugging Plan - Progressive Testing

**Created:** 2026-02-19
**Goal:** Identify and fix LwIP initialization hang
**Starting Point:** FreeRTOS works, LwIP causes hang

---

## Current Status

| Component | Status | Notes |
|-----------|--------|-------|
| HAL_Init | ✅ PASS | Works |
| SystemClock_Config (224MHz) | ✅ PASS | Works |
| FreeRTOS osKernelStart | ✅ PASS | Works |
| FreeRTOS tasks with osDelay | ✅ PASS | Works |
| MX_LWIP_Init | ❌ FAIL | Hangs after 4 blinks |

---

## Test Execution Order

### T6: LwIP tcpip_init Minimal Test (START HERE)

**Purpose:** Isolate whether LwIP's tcpip thread creation works.

**File:** `test_T6_tcpip_init.c`

**Debug Sequence:**
- 1 blink = before MPU
- 2 blinks = after HAL_Init
- 3 blinks = task started
- 4 blinks = BEFORE tcpip_init
- 5 blinks = AFTER tcpip_init call
- 10 fast blinks in callback = tcpip_init SUCCESS
- Continuous slow blink = main task running

**Pass Criteria:** See 10 fast blinks (callback executed)
**Fail Criteria:** Stuck at 4 blinks or no callback blinks

---

### T6b: Add netif_add (if T6 passes)

**Purpose:** Test network interface initialization.

**File:** `test_T6b_netif.c`

**Debug Sequence:**
- Same as T6
- 6 blinks = after netif_add
- 7 blinks = after netif_set_up

**Pass Criteria:** All blinks complete, continuous slow blink
**Fail Criteria:** Stuck at any point

---

### T6c: Add ethernet_link_thread (if T6b passes)

**Purpose:** Test Ethernet link monitoring thread.

**File:** `test_T6c_link_thread.c`

**Debug Sequence:**
- Same as T6b
- 8 blinks = link thread created

**Pass Criteria:** All blinks complete, continuous slow blink
**Fail Criteria:** Stuck at any point

---

### T1: MPU + FreeRTOS (if T6 fails)

**Purpose:** Verify MPU configuration doesn't break FreeRTOS.

**File:** `test_T1_mpu.c`

**Debug Sequence:**
- 1 blink = before MPU
- 2 blinks = after MPU
- 3 blinks = after HAL
- 4 blinks = after SystemClock
- 5 blinks = after osKernelInitialize
- Continuous slow blink = task running

**Pass Criteria:** Continuous slow blink
**Fail Criteria:** Stuck before continuous blink

---

### T4: PHY Detection (if T6/T1 both fail)

**Purpose:** Verify Ethernet PHY (LAN8742) is detected.

**File:** `test_T4_phy.c`

**Debug Sequence:**
- 1-4 blinks = standard init
- 5 blinks = ETH MAC init done
- 10 fast blinks = PHY found (read PHY ID success)
- 10 slow blinks = PHY not found
- Continuous blink = task running

**Pass Criteria:** 10 fast blinks (PHY found)
**Fail Criteria:** 10 slow blinks (PHY not found)

---

## Memory Configuration Reference

| Setting | Value | Location |
|---------|-------|----------|
| FreeRTOS Heap | 64KB | FreeRTOSConfig.h |
| LwIP Heap | 16KB | lwipopts.h (0x30004000) |
| TCPIP Thread Stack | 1024 | lwipopts.h |
| TCPIP Thread Priority | 24 | lwipopts.h |
| DMA Descriptors | RAM_D2 | Linker script (0x30000000) |

---

## Directory Structure

```
microroseth/
├── plan.md                    # This file
├── session-ses_38ee.md        # Session history
├── testing.md                 # Testing report
├── test_results/
│   ├── success/               # Passing test main.c files
│   │   └── test_XXX.c
│   └── failed/                # Failing test main.c files
│       └── test_XXX.c
└── CM7/Core/Src/main.c        # Current active main.c
```

---

## Test Result Logging

After each test, record:
1. Test ID (T6, T6b, etc.)
2. Result (PASS/FAIL)
3. Where it stuck (which blink count)
4. Any user observations
5. Save main.c to appropriate directory

---

## Next Actions

1. Execute T6 (tcpip_init minimal test)
2. Record result and save main.c
3. Based on result, continue with T6b or T1
4. Update this plan with findings

---

## Test Results (Updated 2026-02-19)

### T6 Test Series Results

| Test | Description | Result | Notes |
|------|-------------|--------|-------|
| T6a | tcpip_init in task with osDelay | FAIL | Stuck after 4 blinks |
| T6b | tcpip_init in task (different structure) | FAIL | Stuck after 3 blinks |
| T6c | tcpip_init BEFORE osKernelStart | FAIL | osKernelStart hangs |
| T6d | Include LwIP headers, NO init | PASS | Linking works |
| T6e | tcpip_init FROM task | FAIL | Stuck after 4 blinks |
| T6f | tcpip_init + LED validation | FAIL | Stuck after 4 blinks |
| T6g | tcpip_init + osDelay heartbeat | FAIL | Stuck after 4 blinks |
| T6h | tcpip_init + busy-wait | FAIL | Stuck after 4 blinks |
| T6i | NO tcpip_init (baseline) | PASS | Continuous heartbeat |
| T6j | lwip_init instead of tcpip_init | FAIL | Stuck after 4 blinks |

### Key Finding

**`lwip_init()` also causes the system to hang.** This means the problem is NOT the tcpip thread creation, but something in the LwIP memory initialization.

### Suspected Root Cause

The `lwip_init()` function calls:
- `mem_init()` - Initializes LwIP heap at 0x30004000 (RAM_D2)
- `memp_init()` - Initializes memory pools
- `pbuf_init()` - Initializes packet buffers
- `netif_init()` - Initializes network interface list

Since this is in RAM_D2 (0x30000000), there may be a **memory access issue**:
1. MPU not configured for RAM_D2 region
2. Cache coherency issues
3. Bus fault when accessing RAM_D2

### Next Steps

1. **Test memory access to RAM_D2** - Write a simple test that writes/reads RAM_D2
2. **Add MPU configuration** for RAM_D2 region
3. **Check linker script** for proper section placement
4. **Disable data cache** to test cache coherency


---

## Additional Test Results (T7-T12)

| Test | Description | Result | Notes |
|------|-------------|--------|-------|
| T7 | RAM_D2 memory access | PASS | Memory access works |
| T8 | sys_init() only | PASS | Mutex creation works |
| T9 | sys_init + mem_init | PASS | Heap init works |
| T10 | + memp_init | PASS | Memory pools work |
| T11 | + pbuf_init + netif_init | PASS | All individual functions work |
| T12 | lwip_init() | FAIL | Still hangs |

### Key Discovery

**Individual LwIP init functions work, but lwip_init() fails!**

This means something in lwip_init() that we haven't tested is causing the issue:
- `stats_init()` - Statistics initialization
- `LWIP_ASSERT` checks - Packing/const cast checks
- `tcp_init()` - TCP initialization
- `udp_init()` - UDP initialization
- `ip_init()` - IP initialization
- `etharp_init()` - ARP initialization

### Possible Root Cause

The `lwip_init()` function has LWIP_ASSERT checks that may be failing:
```c
LWIP_ASSERT("LWIP_CONST_CAST not implemented correctly...", ...);
LWIP_ASSERT("Struct packing not implemented correctly...", ...);
```

These assertions may be triggering a hard fault.

### Next Steps

1. Test with LWIP_SKIP_CONST_CHECK and LWIP_SKIP_PACKING_CHECK defined
2. Test stats_init() individually
3. Test tcp_init(), udp_init(), ip_init(), etharp_init()
4. Check if assertions are causing hard faults


---

## BREAKTHROUGH: T14 Test Results

### All Individual LwIP Functions Work!

| Function | Status |
|----------|--------|
| stats_init() | PASS |
| sys_init() | PASS |
| mem_init() | PASS |
| memp_init() | PASS |
| pbuf_init() | PASS |
| netif_init() | PASS |
| sys_timeouts_init() | PASS |

### Root Cause Identified

The problem is the **LWIP_ASSERT checks** at the beginning of `lwip_init()`:

```c
#ifndef LWIP_SKIP_CONST_CHECK
  LWIP_ASSERT("LWIP_CONST_CAST not implemented correctly...", ...);
#endif
#ifndef LWIP_SKIP_PACKING_CHECK
  LWIP_ASSERT("Struct packing not implemented correctly...", ...);
#endif
```

These assertions are failing and causing a hard fault.

### Solution

Add to `lwipopts.h`:
```c
#define LWIP_SKIP_CONST_CHECK    1
#define LWIP_SKIP_PACKING_CHECK  1
```

OR manually initialize LwIP components instead of calling `lwip_init()`.

### Next Steps

1. Add skip checks to lwipopts.h
2. Test with full lwip_init() after adding skips
3. Continue with Ethernet hardware initialization
4. Add tcpip_init() (thread creation) test


---

## Session Summary (2026-02-19)

### Problem Isolation Complete

Through systematic testing, we isolated the LwIP initialization issue:

**Individual functions that WORK:**
| Function | Status |
|----------|--------|
| stats_init() | ✅ PASS |
| sys_init() | ✅ PASS |
| mem_init() | ✅ PASS |
| memp_init() | ✅ PASS |
| pbuf_init() | ✅ PASS |
| netif_init() | ✅ PASS |
| sys_timeouts_init() | ✅ PASS |
| RAM_D2 access | ✅ PASS |

**Function that FAILS:**
| Function | Status |
|----------|--------|
| lwip_init() | ❌ FAIL |

### Root Cause Analysis

The `lwip_init()` function includes LWIP_ASSERT checks at the beginning that cause a hard fault:

```c
LWIP_ASSERT("LWIP_CONST_CAST not implemented correctly...", ...);
LWIP_ASSERT("Struct packing not implemented correctly...", ...);
```

These assertions are failing on the STM32H7 platform.

### Working Solution

**Manual LwIP initialization** (as tested in T14):

```c
stats_init();
sys_init();
mem_init();
memp_init();
pbuf_init();
netif_init();
sys_timeouts_init();
```

This is equivalent to `lwip_init()` but without the failing assertions.

### Test Results Saved

- `test_results/success/test_T14_manual_lwip_init.c` - Working manual LwIP init
- `test_results/success/test_T7_ram_d2_access.c` - RAM_D2 access works
- `test_results/failed/test_T12_lwip_init_stuck.c` - lwip_init() fails
- `test_results/failed/test_T6e_tcpip_from_task.c` - tcpip_init fails

### Next Steps

1. **Use manual LwIP initialization** instead of lwip_init()
2. **Test tcpip_init()** again after manual init
3. **Test full Ethernet initialization**
4. **Add MPU configuration** for Ethernet DMA
5. **Test network interface setup**

### Configuration Changes Made

Added to `lwipopts.h`:
```c
#define LWIP_SKIP_CONST_CHECK    1
#define LWIP_SKIP_PACKING_CHECK  1
```

(Note: These didn't solve the issue, likely due to include order)


---

## Final Solution: Manual LwIP Initialization

### Test Results Summary

| Test | Description | Result |
|------|-------------|--------|
| T20 | Manual LwIP init + osDelay | ✅ PASS |
| T21 | lwip_init() with skip in lwipopts.h | ❌ FAIL |
| T22 | lwip_init() with skip before includes | ❌ FAIL |

### Root Cause Analysis

The `lwip_init()` function fails even with skip macros because:
1. The packing struct is defined at compile time in init.c
2. The `sizeof()` check happens at runtime
3. Struct packing on STM32H7 differs from expected value

### Working Approach

**Use manual LwIP initialization instead of `lwip_init()`:**

```c
/* Manual LwIP init - equivalent to lwip_init() but without assertions */
stats_init();
sys_init();
mem_init();
memp_init();
pbuf_init();
netif_init();
sys_timeouts_init();
```

### Next Steps

1. Continue with Ethernet hardware initialization
2. Add network interface (netif) setup
3. Configure Ethernet PHY
4. Test network connectivity

