#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIMULATION_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$SCRIPT_DIR")")")"

RENODE_SCRIPT="${SIMULATION_DIR}/renode/stm32h7_eth.resc"
DEFAULT_FIRMWARE="${PROJECT_ROOT}/build/stm32h7_eth.elf"

DEFAULT_TIMEOUT=120
HEADLESS=false
INTERACTIVE=false
TIMEOUT_VALUE=""
FIRMWARE_PATH=""
RENODE_ARGS=()

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS] -- [RENODE_ARGS...]

Renode launcher script for STM32H7 Ethernet simulation.

Options:
  -f, --firmware PATH    Path to firmware ELF file (default: build/stm32h7_eth.elf)
  -s, --script PATH      Path to Renode script (default: renode/stm32h7_eth.resc)
  -t, --timeout SECONDS  Timeout for headless execution (default: ${DEFAULT_TIMEOUT}s)
  -H, --headless         Run in headless mode (no GUI, for CI)
  -i, --interactive      Run in interactive mode with Renode shell
  -h, --help             Show this help message

Examples:
  $(basename "$0") -f build/firmware.elf
  $(basename "$0") --headless --timeout 60 -f build/firmware.elf
  $(basename "$0") -i -f build/firmware.elf
  $(basename "$0") -f build/firmware.elf -- --disable-xwt

Exit Codes:
  0  Success
  1  General error
  2  Invalid arguments
  3  Renode not found
  4  Firmware not found
  5  Renode script not found
  6  Timeout exceeded
EOF
}

check_renode() {
    if ! command -v renode &> /dev/null; then
        echo "ERROR: Renode not found in PATH" >&2
        echo "       Install Renode: https://renode.io/downloads/" >&2
        echo "       On macOS: brew install renode" >&2
        return 3
    fi
    return 0
}

check_file() {
    local path="$1"
    local description="$2"
    local exit_code="$3"
    
    if [[ ! -f "$path" ]]; then
        echo "ERROR: ${description} not found: ${path}" >&2
        return "$exit_code"
    fi
    return 0
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -f|--firmware)
                FIRMWARE_PATH="$2"
                shift 2
                ;;
            -s|--script)
                RENODE_SCRIPT="$2"
                shift 2
                ;;
            -t|--timeout)
                TIMEOUT_VALUE="$2"
                shift 2
                ;;
            -H|--headless)
                HEADLESS=true
                shift
                ;;
            -i|--interactive)
                INTERACTIVE=true
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            --)
                shift
                RENODE_ARGS+=("$@")
                break
                ;;
            *)
                echo "ERROR: Unknown option: $1" >&2
                usage >&2
                exit 2
                ;;
        esac
    done
    
    if [[ -z "$FIRMWARE_PATH" ]]; then
        FIRMWARE_PATH="$DEFAULT_FIRMWARE"
    fi
    
    if [[ -z "$TIMEOUT_VALUE" ]]; then
        TIMEOUT_VALUE="$DEFAULT_TIMEOUT"
    fi
}

validate_args() {
    if [[ "$HEADLESS" == true && "$INTERACTIVE" == true ]]; then
        echo "ERROR: Cannot use --headless and --interactive together" >&2
        return 2
    fi
    
    check_renode || return $?
    check_file "$FIRMWARE_PATH" "Firmware" 4 || return $?
    check_file "$RENODE_SCRIPT" "Renode script" 5 || return $?
    
    return 0
}

run_headless() {
    local timeout="$1"
    local log_file="${LOG_FILE:-/tmp/renode_$$_$(date +%s).log}"
    
    echo "Starting Renode in headless mode..."
    echo "  Firmware: $FIRMWARE_PATH"
    echo "  Script: $RENODE_SCRIPT"
    echo "  Timeout: ${timeout}s"
    echo "  Log: $log_file"
    
    local renode_cmd=(
        renode
        --disable-xwt
        --script "$RENODE_SCRIPT"
        "${RENODE_ARGS[@]}"
    )
    
    if [[ "$timeout" -gt 0 ]]; then
        timeout "$timeout" "${renode_cmd[@]}" 2>&1 | tee "$log_file"
        local exit_code=$?
        
        if [[ $exit_code -eq 124 ]]; then
            echo "ERROR: Renode execution timed out after ${timeout}s" >&2
            return 6
        fi
        return $exit_code
    else
        "${renode_cmd[@]}" 2>&1 | tee "$log_file"
        return $?
    fi
}

run_interactive() {
    echo "Starting Renode in interactive mode..."
    echo "  Firmware: $FIRMWARE_PATH"
    echo "  Script: $RENODE_SCRIPT"
    echo ""
    echo "Useful Renode commands:"
    echo "  start              - Start simulation"
    echo "  pause              - Pause simulation"
    echo "  reset              - Reset simulation"
    echo "  quit               - Exit Renode"
    echo "  sysbus LoadELF @<path> - Load firmware"
    echo ""
    
    local renode_cmd=(
        renode
        --script "$RENODE_SCRIPT"
        "${RENODE_ARGS[@]}"
    )
    
    "${renode_cmd[@]}"
    return $?
}

run_default() {
    echo "Starting Renode..."
    echo "  Firmware: $FIRMWARE_PATH"
    echo "  Script: $RENODE_SCRIPT"
    
    local renode_cmd=(
        renode
        --script "$RENODE_SCRIPT"
        "${RENODE_ARGS[@]}"
    )
    
    "${renode_cmd[@]}"
    return $?
}

main() {
    parse_args "$@"
    validate_args || exit $?
    
    if [[ "$HEADLESS" == true ]]; then
        run_headless "$TIMEOUT_VALUE"
        exit $?
    elif [[ "$INTERACTIVE" == true ]]; then
        run_interactive
        exit $?
    else
        run_default
        exit $?
    fi
}

main "$@"
