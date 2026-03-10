---
title: CM7
parent: Firmware
nav_order: 2
---

# CM7 Boot Flow

The CM7 smoke firmware is the "release" core for dual-core bring-up. It initializes UART2 and GPIO, releases CM4 using HSEM, then waits for an ACK in the shared mailbox.

## HSEM Release Sequence

1. Initialize GPIO and UART2 for console output.
2. Enable HSEM clock.
3. Set mailbox to empty.
4. Release CM4 with `HSEM_FastTakeRelease(0U)`.
5. Write mailbox value `MAILBOX_CM7_RELEASED`.
6. Busy-wait until CM4 writes `MAILBOX_CM4_ACKED`.

## Empty Loop Note

The CM7 waits in a tight loop for CM4 to ACK. This is an intentional blocking wait for deterministic bring-up in the Renode smoke firmware. If this is used on hardware, consider adding a timeout or watchdog integration.

Sources:
- `Test_Board_Sensore/CM7/Core/Src/main_smoke_dualcore_cm7.c`
