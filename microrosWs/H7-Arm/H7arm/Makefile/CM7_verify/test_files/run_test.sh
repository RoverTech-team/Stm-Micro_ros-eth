#!/bin/bash
# Test Runner Script
# Usage: ./run_test.sh T1  (runs T1_MPU_test.c)

TEST_DIR="$(dirname "$0")"
MAIN_DIR="$(dirname "$TEST_DIR")"
TEST_NAME=$1

if [ -z "$TEST_NAME" ]; then
    echo "Available tests:"
    ls -1 "$TEST_DIR"/*.c 2>/dev/null | xargs -n1 basename | sed 's/.c$//'
    echo ""
    echo "Usage: ./run_test.sh <test_name>"
    echo "Example: ./run_test.sh T1_MPU_test"
    exit 1
fi

TEST_FILE="$TEST_DIR/${TEST_NAME}.c"

if [ ! -f "$TEST_FILE" ]; then
    echo "ERROR: Test file not found: $TEST_FILE"
    exit 1
fi

echo "=========================================="
echo "Running test: $TEST_NAME"
echo "=========================================="

# Backup current main.c
if [ -f "$MAIN_DIR/../../CM7/Core/Src/main.c" ]; then
    cp "$MAIN_DIR/../../CM7/Core/Src/main.c" "$MAIN_DIR/../../CM7/Core/Src/main.c.backup"
    echo "Backed up main.c"
fi

# Copy test file to main.c
cp "$TEST_FILE" "$MAIN_DIR/../../CM7/Core/Src/main.c"
echo "Copied $TEST_NAME.c to main.c"

# Build
echo ""
echo "Building..."
cd "$MAIN_DIR"
make clean
make -j4

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    exit 1
fi

echo ""
echo "Build successful!"
echo ""
echo "Now flash with:"
echo "  CLI=\"/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin/STM32_Programmer_CLI\""
echo "  \$CLI -c port=SWD -w build/MicroRosEth_CM7.elf -v -rst"
echo ""
echo "Expected blink sequence for $TEST_NAME:"
case $TEST_NAME in
    T1_MPU_test)
        echo "  1 blink  = before MPU"
        echo "  2 blinks = after MPU"
        echo "  3 blinks = after HAL + Clock"
        echo "  5 blinks = task started"
        echo "  Continuous slow = SUCCESS"
        ;;
    T2_tcpip_init_test)
        echo "  1-3 blinks = standard init"
        echo "  4 blinks = task started"
        echo "  5 FAST blinks = tcpip_init SUCCESS"
        echo "  5 SLOW blinks = tcpip_init FAILED"
        echo "  6 blinks = continuing"
        echo "  Continuous slow = main loop running"
        ;;
    T3_netif_add_test)
        echo "  1-5 blinks = init + tcpip"
        echo "  6 blinks = before netif_add"
        echo "  7 blinks = netif_add SUCCESS"
        echo "  8 blinks = netif up"
        echo "  Continuous slow = main loop"
        echo "  If stuck at 6 blinks: netif_add() hangs"
        ;;
    T4_full_debug_test)
        echo "  1-4 blinks = init"
        echo "  See T4b for internal lwip.c debug blinks"
        echo "  10 fast blinks = full SUCCESS"
        echo "  Continuous slow = main loop"
        ;;
esac
