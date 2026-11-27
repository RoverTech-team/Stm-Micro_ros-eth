#!/bin/bash
# E2E: CM7 shared J1-J5 (J6 forced 0) + CM4 follow (J6 skipped).
# CM7 e2e @0x08000000 + CM4 follow @0x08100000.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BRINGUP="$(dirname "$SCRIPT_DIR")"
CLI="${CLI:-/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI}"

echo "=== Building cm7_e2e ==="
make -C "$BRINGUP/cm7_e2e" -j4
echo "=== Building cm4_follow ==="
make -C "$BRINGUP/cm4_follow" -j4
echo "=== Mass erase ==="
"$CLI" -c port=SWD mode=UR -e all
echo "=== Flash CM7 e2e ==="
"$CLI" -c port=SWD mode=UR -w "$BRINGUP/cm7_e2e/build/MicroRosEth_CM7_e2e.elf" -v
echo "=== Flash CM4 follow ==="
"$CLI" -c port=SWD mode=UR -w "$BRINGUP/cm4_follow/build/MicroRosEth_CM4_follow.elf" -v
"$CLI" -c port=SWD -rst > /dev/null
echo "DONE: e2e stack flashed (verify ping + joint-cmd-shared on serial)"
