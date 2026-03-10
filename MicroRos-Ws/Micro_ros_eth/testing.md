# STM32H755 CM7 Firmware Testing Report

**Date:** 2026-02-18  
**Target:** STM32H755ZIT6 (Nucleo-H755ZI-Q) - CM7 Core Only  
**Goal:** Boot CM7 firmware independently without CM4 firmware

---

## Summary

After extensive debugging, the CM7 firmware can now boot and run FreeRTOS successfully. The main issues were related to boot sequence, FPU configuration, and component initialization order.

---

## What Was Tested

### 1. Boot Sequence
| Test | Result | Notes |
|------|--------|-------|
| Minimal register-level blink | ✅ PASS | Direct GPIO/RCC register access works |
| HAL_Init() | ✅ PASS | HAL initialization works |
| SystemClock_Config() (PLL 224MHz) | ✅ PASS | PLL configuration works |
| VTOR relocation (0x08000000) | ✅ PASS | Vector table correctly set |
| FreeRTOS osKernelStart() | ✅ PASS | Scheduler starts successfully |
| FreeRTOS task execution | ✅ PASS | Tasks run with osDelay() |

### 2. Component Initialization
| Component | Status | Notes |
|-----------|--------|-------|
| MPU_Config() | ⚠️ UNCERTAIN | Works without LwIP, may cause issues with LwIP |
| MX_LWIP_Init() | ❌ FAILS | Causes firmware to hang after initialization |
| Ethernet PHY | ❓ UNTESTED | Not reached due to LwIP issue |

### 3. Configuration Fixes Applied
| Fix | File | Status |
|-----|------|--------|
| Enable USER_VECT_TAB_ADDRESS | system_stm32h7xx_dualcore_boot_cm4_cm7.c | ✅ Applied |
| Fix FPU in startup (.fpu fpv5-d16) | startup_stm32h755xx_CM7.s | ✅ Applied |
| Enable FPU in FreeRTOS (configENABLE_FPU=1) | FreeRTOSConfig.h | ✅ Applied |
| GPIO LED pins initialization | gpio.c / main.c | ✅ Fixed |
| CM4 sync timeout non-fatal | main.c | ✅ Applied |
| SCB->VTOR explicit setting | main.c | ✅ Applied |

---

## What Was Validated to Work

### Minimal Firmware (No HAL, No FreeRTOS)
```c
RCC->AHB4ENR |= (1 << 1);  // GPIOB clock
GPIOB->MODER |= (1 << 0);   // PB0 output
while(1) { /* blink with busy-wait */ }
```
**Result:** ✅ Works reliably

### HAL + PLL Clock (No FreeRTOS)
```c
HAL_Init();
SystemClock_Config();  // 224 MHz from PLL
SCB->VTOR = 0x08000000;
while(1) { /* blink */ }
```
**Result:** ✅ Works reliably

### Full FreeRTOS (No LwIP)
```c
HAL_Init();
SystemClock_Config();
SCB->VTOR = 0x08000000;
osKernelInitialize();
osThreadNew(task, ...);
osKernelStart();
// Task: blink with osDelay()
```
**Result:** ✅ Works reliably

---

## Known Problems

### 1. LwIP Initialization Hangs Firmware
**Symptom:** After calling `MX_LWIP_Init()`, firmware stops responding. LED stops blinking.

**Possible Causes:**
- MPU configuration conflicts with Ethernet DMA buffers
- LwIP tcpip thread not starting properly
- Ethernet PHY initialization blocking
- Memory alignment issues with DMA descriptors

**Investigation Needed:**
- Check if `tcpip_init()` callback is being called
- Verify Ethernet DMA descriptor placement in RAM_D2 (0x30000000)
- Check if PHY is being detected (LAN8742)
- Verify MPU region configuration for Ethernet buffers

### 2. osDelay() Behavior After LwIP Init
**Symptom:** When `osDelay()` is used after `MX_LWIP_Init()`, task appears to hang.

**Possible Causes:**
- LwIP creates its own threads that may be consuming all FreeRTOS heap
- Stack overflow in LwIP threads
- SysTick interrupt being affected

### 3. MPU Configuration Uncertainty
**Symptom:** MPU_Config() works alone, but combined with LwIP may cause issues.

**Current Config:**
```c
MPU region 0: 0x30000000, 256KB, Non-cacheable, Non-bufferable
```

**Possible Issues:**
- Region size may be incorrect
- May need additional MPU regions for proper LwIP operation

---

## Recommended Next Steps

### Priority 1: Fix LwIP Initialization
1. Add debug output (GPIO blinks) inside LwIP init to find exact hang point
2. Check if `tcpip_init_done_callback()` is being called
3. Verify Ethernet MAC/PHY clock is enabled
4. Test with minimal LwIP config (no DHCP, static IP only)

### Priority 2: Verify Ethernet Hardware
1. Check if Ethernet link LED on board lights up
2. Verify PHY address (LAN8742 typically at address 0)
3. Test PHY register read/write

### Priority 3: Memory Configuration
1. Verify DMA descriptors are in correct memory region
2. Check linker script for proper section placement
3. Ensure enough heap for LwIP + FreeRTOS

---

## Working Code Reference

### Current Working Main.c (FreeRTOS Only)
```c
#include "main.h"
#include "cmsis_os.h"

void SystemClock_Config(void) { /* PLL config for 224MHz */ }

osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 2048,
  .priority = osPriorityNormal,
};

void StartDefaultTask(void *argument) {
  for(;;) {
    GPIOB->BSRR = (1 << 0);
    osDelay(500);
    GPIOB->BSRR = (1 << 16);
    osDelay(500);
  }
}

int main(void) {
  RCC->AHB4ENR |= (1 << 1);
  GPIOB->MODER &= ~(3 << 0);
  GPIOB->MODER |= (1 << 0);

  HAL_Init();
  SystemClock_Config();
  SCB->VTOR = 0x08000000;

  osKernelInitialize();
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  osKernelStart();

  while(1);
}
```

---

## Hardware Configuration

| Setting | Value |
|---------|-------|
| MCU | STM32H755ZIT6 Rev V |
| Board | NUCLEO-H755ZI-Q |
| Core | Cortex-M7 only (CM4 not flashed) |
| System Clock | 224 MHz (HSI + PLL) |
| LED Green | PB0 |
| LED Red | PB14 |
| Ethernet PHY | LAN8742 |

---

## Files Modified

| File | Change |
|------|--------|
| `Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c` | Enabled USER_VECT_TAB_ADDRESS |
| `Makefile/CM7/startup_stm32h755xx_CM7.s` | Changed .fpu to fpv5-d16 |
| `CM7/Core/Inc/FreeRTOSConfig.h` | Set configENABLE_FPU=1 |
| `CM7/Core/Src/gpio.c` | Added LED pin initialization |
| `CM7/Core/Src/main.c` | Complete rewrite with debug sequence |

---

## Conclusion

The CM7 core can boot and run FreeRTOS successfully without CM4 firmware. The main blocker for full micro-ROS functionality is LwIP/Ethernet initialization. Further investigation is needed to identify why LwIP initialization causes the firmware to hang.

---

## Progressive Test Files

Test files are located in: `Makefile/CM7/test_files/`

| File | Purpose | Expected Blink Sequence |
|------|---------|------------------------|
| T1_MPU_test.c | Test MPU configuration | 1→2→3→5→continuous |
| T2_tcpip_init_test.c | Test LwIP tcpip_init | 1→2→3→4→5(fast=OK/slow=FAIL)→6→continuous |
| T3_netif_add_test.c | Test netif_add | 1→2→3→4→5→6→7→8→continuous |
| T4_full_debug_test.c | Full LwIP with debug | See T4b patch |
| T4b_lwip_debug_patch.c | Debug patch for lwip.c | Add blinks inside MX_LWIP_Init |
| run_test.sh | Test runner script | Automates test execution |

---

## How to Run Tests

### Manual Method
```bash
cd /Users/giuliomastromartino/Documents/Polispace/DEV/MICRO_ROS_ETH-main/microrosWs/Micro_ros_eth/microroseth

# 1. Copy test file to main.c
cp Makefile/CM7/test_files/T1_MPU_test.c CM7/Core/Src/main.c

# 2. Build
cd Makefile/CM7
make clean && make -j4

# 3. Flash
CLI="/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin/STM32_Programmer_CLI"
$CLI -c port=SWD -w build/MicroRosEth_CM7.elf -v -rst

# 4. Observe LED blinks
```

### Automated Method
```bash
cd /Users/giuliomastromartino/Documents/Polispace/DEV/MICRO_ROS_ETH-main/microrosWs/Micro_ros_eth/microroseth/Makefile/CM7/test_files
./run_test.sh T1_MPU_test
```

---

## Test Execution Order

1. **T1** → Confirms MPU + FreeRTOS works
2. **T2** → Confirms tcpip_init works  
3. **T3** → Confirms netif_add works
4. **T4 + T4b** → Full debug to find exact hang point

---

## Decision Matrix

### If T1 Fails (after 1-2 blinks)
- **Problem:** MPU configuration
- **Solution:** Disable MPU or adjust region settings

### If T2 Fails (5 slow blinks)
- **Problem:** tcpip_init not completing
- **Solution:** Check FreeRTOS heap size, check tcpip thread creation

### If T3 Fails (stuck at 6 blinks)
- **Problem:** netif_add or ethernetif_init hangs
- **Solution:** Debug inside ethernetif_init(), check ETH MAC init, check PHY

### If T4 Shows All Blinks but Then Hangs
- **Problem:** ethernet_link_thread or network link check
- **Solution:** Check thread stack size, check link polling

---

## Configuration Fixes Summary

Based on analysis, these configuration changes should be applied:

### FreeRTOSConfig.h Changes
```c
// BEFORE:
#define configTOTAL_HEAP_SIZE  ((size_t)65536)

// AFTER (RECOMMENDED):
#define configTOTAL_HEAP_SIZE  ((size_t)131072)  // 128KB for LwIP + micro-ROS
#define configENABLE_MPU  1  // Enable if using MPU
```

### lwipopts.h Changes
```c
// Increase thread stacks
#define TCPIP_THREAD_STACKSIZE  2048  // 8KB (was 1024)
#define DEFAULT_THREAD_STACKSIZE 2048  // 8KB (was 1024)
```

### ethernetif.c Changes
```c
// Increase ethernet thread stack
#define INTERFACE_THREAD_STACK_SIZE ( 1024 )  // 4KB (was 350)
```

---

## Quick Reference

### Blink Code Reference

| Blinks | Meaning |
|--------|---------|
| 1 | Starting / before component |
| 2 | After component A |
| 3 | After component B |
| 4 | Task started / before LwIP |
| 5 | FAST = callback SUCCESS, SLOW = timeout FAIL |
| 6+ | Progress through init stages |
| Continuous slow | Main loop running (SUCCESS) |
| Continuous fast | Error state |
| Solid ON | Hung in busy-wait or early crash |

### Memory Map

| Region | Address | Size | Usage |
|--------|---------|------|-------|
| DTCMRAM | 0x20000000 | 128KB | Stack |
| RAM_D1 | 0x24000000 | 512KB | .data, .bss, heap |
| RAM_D2 | 0x30000000 | 288KB | Ethernet DMA |
| FLASH | 0x08000000 | 1MB | Code |

### Key Files

| File | Purpose |
|------|---------|
| `Makefile/CM7/stm32h755xx_flash_CM7.ld` | Linker script |
| `CM7/Core/Inc/FreeRTOSConfig.h` | FreeRTOS settings |
| `CM7/LWIP/Target/lwipopts.h` | LwIP settings |
| `CM7/LWIP/App/lwip.c` | LwIP initialization |
| `CM7/LWIP/Target/ethernetif.c` | Ethernet driver |
| `Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c` | System init |

---

## Test Results Summary

**Date:** 2026-02-18 (Session 2)

### Test Execution Results

| Test | Description | Result | Notes |
|------|-------------|--------|-------|
| T1 | MPU + FreeRTOS | ✅ PASS | MPU configuration works |
| T2 | tcpip_init callback | ❌ FAIL | Callback never called |
| T2b | tcpip_init (no wait loop) | ❌ FAIL | Same - callback not called |
| T2c | FreeRTOS thread creation | ✅ PASS | Can create threads without LwIP |
| T3 | netif_add | ⏭️ SKIPPED | Requires tcpip_init which fails |

### Root Cause Analysis

**The problem is specifically in LwIP's `tcpip_init()` function:**

1. ✅ FreeRTOS scheduler starts successfully
2. ✅ FreeRTOS can create new threads dynamically
3. ✅ `tcpip_init()` returns (doesn't hang)
4. ❌ The tcpip thread callback is **never called**
5. This means either:
   - The tcpip thread fails to create silently
   - The tcpip thread crashes immediately after creation

### FreeRTOS Heap Test

Changed from 64KB → 128KB - did not fix the issue.

### Next Steps Required

1. **Debug inside tcpip_init():**
   - Add debug output to `sys_thread_new()` in `sys_arch.c`
   - Check if `osThreadNew()` returns NULL
   - Check if the thread function starts

2. **Alternative approaches:**
   - Try LwIP without RTOS (NO_SYS=1) 
   - Use different LwIP version
   - Check if TCPIP_THREAD_PRIO is too high (currently 24)

3. **Check tcpip thread stack:**
   - Currently 1024 words = 4KB
   - May need to increase

---

## Recommended Actions

### Option A: Debug LwIP Thread Creation
Add debug to `sys_arch.c`:
```c
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
  // Add debug: blink LED before thread create
  GPIOB->BSRR = (1 << 0);
  for(volatile int i = 0; i < 200000; i++);
  GPIOB->BSRR = (1 << 16);
  
  const osThreadAttr_t attributes = {
    .name = name,
    .stack_size = stacksize,
    .priority = (osPriority_t)prio,
  };
  sys_thread_t result = osThreadNew(thread, arg, &attributes);
  
  // Add debug: blink after thread create
  if(result == NULL) {
    // 2 fast blinks = NULL returned
    for(int j = 0; j < 2; j++) {
      GPIOB->BSRR = (1 << 0); for(volatile int i = 0; i < 200000; i++);
      GPIOB->BSRR = (1 << 16); for(volatile int i = 0; i < 200000; i++);
    }
  } else {
    // 2 slow blinks = thread created
    for(int j = 0; j < 2; j++) {
      GPIOB->BSRR = (1 << 0); for(volatile int i = 0; i < 500000; i++);
      GPIOB->BSRR = (1 << 16); for(volatile int i = 0; i < 500000; i++);
    }
  }
  
  return result;
}
```

### Option B: Increase Thread Stack
In `lwipopts.h`:
```c
#define TCPIP_THREAD_STACKSIZE  2048  // 8KB instead of 4KB
```

### Option C: Lower Thread Priority
In `lwipopts.h`:
```c
#define TCPIP_THREAD_PRIO  3  // Normal priority instead of 24
```

