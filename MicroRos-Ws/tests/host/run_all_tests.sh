#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
REPORT_DIR="${SCRIPT_DIR}/test_reports"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
REPORT_FILE="${REPORT_DIR}/test_report_${TIMESTAMP}.txt"

VERBOSE=0
RUN_COVERAGE=0
RUN_VALGRIND=0
RUN_STATIC_ANALYSIS=0

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -v          Verbose output"
    echo "  -c          Run with code coverage"
    echo "  -m          Run with valgrind memory check (Linux only)"
    echo "  -s          Run static analysis"
    echo "  -a          Run all checks (coverage, valgrind, static analysis)"
    echo "  -h          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                  # Run all tests"
    echo "  $0 -v               # Run tests with verbose output"
    echo "  $0 -c               # Run tests with coverage report"
    echo "  $0 -m               # Run tests with valgrind"
    echo "  $0 -a               # Run all checks"
}

while getopts "vcmsah" opt; do
    case $opt in
        v)
            VERBOSE=1
            ;;
        c)
            RUN_COVERAGE=1
            ;;
        m)
            RUN_VALGRIND=1
            ;;
        s)
            RUN_STATIC_ANALYSIS=1
            ;;
        a)
            RUN_COVERAGE=1
            RUN_VALGRIND=1
            RUN_STATIC_ANALYSIS=1
            ;;
        h)
            print_usage
            exit 0
            ;;
        \?)
            print_usage
            exit 1
            ;;
    esac
done

mkdir -p "${REPORT_DIR}"

echo "========================================="
echo "  micro-ROS STM32H7 Test Suite"
echo "  $(date)"
echo "========================================="
echo ""

TOTAL_TESTS=0
TOTAL_PASSED=0
TOTAL_FAILED=0
TOTAL_ERRORS=0

run_test() {
    local test_name=$1
    local test_runner="${BUILD_DIR}/${test_name}_runner"
    
    if [ ! -f "${test_runner}" ]; then
        echo "[ERROR] Test runner not found: ${test_runner}"
        TOTAL_ERRORS=$((TOTAL_ERRORS + 1))
        return 1
    fi
    
    echo "----------------------------------------"
    echo "Running: ${test_name}"
    echo "----------------------------------------"
    
    local output
    local exit_code
    
    if [ $RUN_VALGRIND -eq 1 ] && command -v valgrind >/dev/null 2>&1; then
        output=$(valgrind --leak-check=full --error-exitcode=1 --suppressions="${SCRIPT_DIR}/valgrind.supp" "${test_runner}" 2>&1)
        exit_code=$?
    else
        output=$("${test_runner}" 2>&1)
        exit_code=$?
    fi
    
    if [ $VERBOSE -eq 1 ]; then
        echo "${output}"
    else
        echo "${output}" | tail -20
    fi
    
    local tests_run=$(echo "${output}" | grep -E "^[0-9]+ Tests" | sed 's/ Tests.*//' | head -1)
    local tests_failed=$(echo "${output}" | grep -oE "[0-9]+ Failures" | sed 's/ Failures//')
    
    if [ -z "$tests_run" ]; then tests_run=0; fi
    if [ -z "$tests_failed" ]; then tests_failed=0; fi
    
    local tests_passed=$((tests_run - tests_failed))
    
    TOTAL_TESTS=$((TOTAL_TESTS + tests_run))
    TOTAL_PASSED=$((TOTAL_PASSED + tests_passed))
    TOTAL_FAILED=$((TOTAL_FAILED + tests_failed))
    
    if [ $exit_code -eq 0 ]; then
        echo "[PASS] ${test_name}: ${tests_passed}/${tests_run} tests passed"
    else
        echo "[FAIL] ${test_name}: ${tests_failed} tests failed"
    fi
    echo ""
}

echo "=== Building Tests ==="
echo ""

cd "$SCRIPT_DIR"

if [ $RUN_COVERAGE -eq 1 ]; then
    make clean >/dev/null 2>&1
    if [ $VERBOSE -eq 1 ]; then
        make coverage-build
    else
        make coverage-build >/dev/null 2>&1
    fi
else
    make clean >/dev/null 2>&1
    if [ $VERBOSE -eq 1 ]; then
        make
    else
        make >/dev/null 2>&1
    fi
fi

BUILD_RESULT=$?
if [ $BUILD_RESULT -ne 0 ]; then
    echo "[ERROR] Build failed with code ${BUILD_RESULT}"
    exit 1
fi

echo "Build successful."
echo ""

if [ $RUN_STATIC_ANALYSIS -eq 1 ]; then
    echo "=== Running Static Analysis ==="
    echo ""
    make static-analysis 2>&1 | tee -a "${REPORT_FILE}"
    echo ""
fi

echo "=== Running Tests ==="
echo ""

run_test "test_memory"
run_test "test_ip_config"
run_test "test_transports"
run_test "test_udp_transport"
run_test "test_microros_allocators"

if [ $RUN_COVERAGE -eq 1 ]; then
    echo "=== Generating Coverage Report ==="
    echo ""
    make coverage-report 2>&1 | tee -a "${REPORT_FILE}"
    echo ""
fi

echo "========================================="
echo "  Test Summary"
echo "========================================="
echo "Total Tests:  ${TOTAL_TESTS}"
echo "Passed:       ${TOTAL_PASSED}"
echo "Failed:       ${TOTAL_FAILED}"
echo "Errors:       ${TOTAL_ERRORS}"
echo ""

if [ ${TOTAL_FAILED} -eq 0 ] && [ ${TOTAL_ERRORS} -eq 0 ]; then
    echo "=== ALL TESTS PASSED ==="
    echo ""
    echo "Report saved to: ${REPORT_FILE}"
    exit 0
else
    echo "=== SOME TESTS FAILED ==="
    echo ""
    echo "Report saved to: ${REPORT_FILE}"
    exit 1
fi
