#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
VERBOSE=0

while getopts "v" opt; do
    case $opt in
        v)
            VERBOSE=1
            ;;
        \?)
            echo "Usage: $0 [-v]"
            exit 1
            ;;
    esac
done

echo "=== Building tests ==="
cd "$SCRIPT_DIR"

if [ $VERBOSE -eq 1 ]; then
    make clean 2>/dev/null
    make
else
    make > /dev/null 2>&1
fi

BUILD_RESULT=$?

if [ $BUILD_RESULT -ne 0 ]; then
    echo "ERROR: Build failed with code $BUILD_RESULT"
    exit $BUILD_RESULT
fi

echo "Build successful."
echo ""

echo "=== Running tests ==="
if [ ! -f "${BUILD_DIR}/test_runner" ]; then
    echo "ERROR: test_runner executable not found in ${BUILD_DIR}"
    exit 1
fi

if [ $VERBOSE -eq 1 ]; then
    "${BUILD_DIR}/test_runner" -v
else
    "${BUILD_DIR}/test_runner"
fi

TEST_RESULT=$?
echo ""

if [ $TEST_RESULT -eq 0 ]; then
    echo "=== All tests PASSED ==="
else
    echo "=== Some tests FAILED ==="
fi

exit $TEST_RESULT