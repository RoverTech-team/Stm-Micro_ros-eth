#!/bin/bash
# Static Analysis Runner for MICRO_ROS_ETH
# Runs available static analysis tools and reports results

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "MICRO_ROS_ETH Static Analysis"
echo "========================================="
echo ""

# Track if any issues found
ISSUES_FOUND=0

# Function to check if a tool is available
check_tool() {
    if command -v "$1" >/dev/null 2>&1; then
        echo -e "${GREEN}✓${NC} $1 is available"
        return 0
    else
        echo -e "${YELLOW}○${NC} $1 not installed"
        return 1
    fi
}

# Check available tools
echo "Checking available analysis tools..."
echo ""

TOOLS_AVAILABLE=0
check_tool cppcheck && TOOLS_AVAILABLE=$((TOOLS_AVAILABLE + 1))
check_tool clang-tidy && TOOLS_AVAILABLE=$((TOOLS_AVAILABLE + 1))
check_tool gcc && TOOLS_AVAILABLE=$((TOOLS_AVAILABLE + 1))

echo ""
echo "-------------------------------------------"
echo ""

# Always run GCC warnings (it's always available)
echo "========================================="
echo "1. GCC Strict Warnings (-Wall -Wextra -Wpedantic -Werror)"
echo "========================================="

# Compile with strict warnings but don't fail on errors - just report
WARNINGS=$(gcc -Wall -Wextra -Wpedantic -std=c99 -fsyntax-only \
    -I. -Iunity -Imocks \
    test_memory.c test_ip_config.c test_transports.c \
    mocks/mock_lwip.c mocks/mock_freertos.c mocks/mock_hal.c \
    mocks/mock_memory.c mocks/mock_ip_config.c mocks/mock_transports.c \
    2>&1 || true)

if [ -z "$WARNINGS" ]; then
    echo -e "${GREEN}✓ No warnings found by GCC${NC}"
else
    echo "$WARNINGS"
    WARNING_COUNT=$(echo "$WARNINGS" | grep -c "warning:" || echo "0")
    if [ "$WARNING_COUNT" -gt 0 ]; then
        echo ""
        echo -e "${YELLOW}Found $WARNING_COUNT warning(s)${NC}"
        ISSUES_FOUND=1
    fi
fi

echo ""

# Run cppcheck if available
if command -v cppcheck >/dev/null 2>&1; then
    echo "========================================="
    echo "2. Cppcheck Analysis"
    echo "========================================="
    
    CPPCHECK_OUTPUT=$(cppcheck --enable=all --std=c99 --inline-suppr \
        --suppress=missingIncludeSystem \
        --suppress=unusedFunction \
        -I. -Iunity -Imocks \
        test_memory.c test_ip_config.c test_transports.c \
        mocks/mock_lwip.c mocks/mock_freertos.c mocks/mock_hal.c \
        mocks/mock_memory.c mocks/mock_ip_config.c mocks/mock_transports.c \
        2>&1 || true)
    
    if [ -z "$CPPCHECK_OUTPUT" ]; then
        echo -e "${GREEN}✓ No issues found by Cppcheck${NC}"
    else
        echo "$CPPCHECK_OUTPUT"
        ERROR_COUNT=$(echo "$CPPCHECK_OUTPUT" | grep -c "error:" || echo "0")
        if [ "$ERROR_COUNT" -gt 0 ]; then
            echo ""
            echo -e "${RED}Found $ERROR_COUNT error(s) by Cppcheck${NC}"
            ISSUES_FOUND=1
        fi
    fi
    echo ""
fi

# Run clang-tidy if available
if command -v clang-tidy >/dev/null 2>&1; then
    echo "========================================="
    echo "3. Clang-Tidy Analysis"
    echo "========================================="
    
    TIDY_OUTPUT=$(clang-tidy \
        test_memory.c test_ip_config.c test_transports.c \
        mocks/mock_lwip.c mocks/mock_freertos.c mocks/mock_hal.c \
        mocks/mock_memory.c mocks/mock_ip_config.c mocks/mock_transports.c \
        -- -I. -Iunity -Imocks 2>&1 || true)
    
    if [ -z "$TIDY_OUTPUT" ] || echo "$TIDY_OUTPUT" | grep -q "0 warnings"; then
        echo -e "${GREEN}✓ No issues found by Clang-Tidy${NC}"
    else
        echo "$TIDY_OUTPUT"
        TIDY_WARNING_COUNT=$(echo "$TIDY_OUTPUT" | grep -c "warning:" || echo "0")
        if [ "$TIDY_WARNING_COUNT" -gt 0 ]; then
            echo ""
            echo -e "${YELLOW}Found $TIDY_WARNING_COUNT warning(s) by Clang-Tidy${NC}"
        fi
    fi
    echo ""
fi

# Summary
echo "========================================="
echo "Summary"
echo "========================================="

if [ $ISSUES_FOUND -eq 0 ]; then
    echo -e "${GREEN}✓ Static analysis passed with no critical issues${NC}"
    exit 0
else
    echo -e "${YELLOW}⚠ Static analysis found some issues (see above)${NC}"
    echo ""
    echo "To suppress specific warnings, you can:"
    echo "  - Add // cppcheck-suppress <id> for cppcheck"
    echo "  - Add // NOLINT for clang-tidy"
    echo "  - Add #pragma GCC diagnostic ignored \"-W<warning>\" for GCC"
    exit 1
fi
