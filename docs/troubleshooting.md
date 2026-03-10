---
title: Troubleshooting
nav_order: 7
---

# Global Troubleshooting

## Boot Issues

1. CM4 never starts
   Check CM7 HSEM release sequence and mailbox ACK (`main_smoke_dualcore_cm7.c`).

2. CM4 vector table mismatch
   Ensure CM4 VTOR is set to `0x08100000` and the CM4 linker script targets flash bank 2.

## Timer and Sensor Issues

1. No echo detected
   Verify TRIG/ECHO pins (PD1/PD0) and check timeout constants in `CM4/Core/Src/main.c`.

2. Distance blinks are zero or stuck
   Confirm `echo_time / 58` conversion and timer base rate (`TIMER_TARGET_HZ`).

## Renode Issues

1. Script exits early
   Confirm Renode path and that required ELFs are built.

2. No UART output
   Use validation scripts that mirror USART to console logs.

## Agent and Network Issues

1. XRCE-DDS session never starts
   Check agent UDP port (`8888`) and TAP gateway IP (`192.168.50.1`).

2. Dashboard empty
   Verify `microk3` and `renode-bridge` containers are healthy and topics match defaults.
