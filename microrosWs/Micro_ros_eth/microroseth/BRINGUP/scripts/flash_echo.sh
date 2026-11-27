#!/bin/bash
# Rollback / baseline: proven minimal echo (ping + joint echo, no shared motion).
# CM7 echo @0x08000000 + CM4 follow @0x08100000 (CM4 holds, J1-J5 cmds stay 0).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BRINGUP="$(dirname "$SCRIPT_DIR")"
CLI="${CLI:-/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI}"

echo "=== Building cm7_echo ==="
make -C "$BRINGUP/cm7_echo" -j4
echo "=== Building cm4_follow ==="
make -C "$BRINGUP/cm4_follow" -j4
echo "=== Mass erase ==="
"$CLI" -c port=SWD mode=UR -e all
echo "=== Flash CM7 echo ==="
"$CLI" -c port=SWD mode=UR -w "$BRINGUP/cm7_echo/build/MicroRosEth_CM7_echo.elf" -v
echo "=== Flash CM4 follow ==="
"$CLI" -c port=SWD mode=UR -w "$BRINGUP/cm4_follow/build/MicroRosEth_CM4_follow.elf" -v
"$CLI" -c port=SWD -rst > /dev/null
echo "DONE: echo stack flashed (ping 192.168.0.5 expected OK)"
