#!/bin/bash
#
# TAP Test Runner for Renode micro-ROS Simulation
#
# This script orchestrates the complete TAP-based networking test:
#   1. Setup TAP interface
#   2. Start micro-ROS agent with IP alias
#   3. Run Renode simulation
#   4. Check for XRCE-DDS communication
#   5. Cleanup
#
# Network topology:
#   STM32 (192.168.0.3) <---> TAP (192.168.0.1) <---> Agent (192.168.0.8:8888)
#
# Usage: sudo ./run_tap_test.sh [OPTIONS]
#

set -euo pipefail

# ========================================
# Configuration
# ========================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIMULATION_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

# Network configuration
TAP_INTERFACE="${TAP_INTERFACE:-tap0}"
GATEWAY_IP="${GATEWAY_IP:-192.168.0.1}"
AGENT_IP="${AGENT_IP:-192.168.0.8}"
DEVICE_IP="${DEVICE_IP:-192.168.0.3}"
AGENT_PORT="${AGENT_PORT:-8888}"
NETMASK="${NETMASK:-255.255.255.0}"

# Timing
SIM_DURATION="${SIM_DURATION:-60}"
AGENT_WAIT="${AGENT_WAIT:-5}"

# Paths
RENODE_PATH="${PROJECT_ROOT}/Renode.app/Contents/MacOS/renode"
FIRMWARE_PATH="${PROJECT_ROOT}/Micro_ros_eth/microroseth/Makefile/CM7/build/MicroRosEth_CM7.elf"
TAP_RESC="${SIMULATION_DIR}/renode/microros_tap.resc"
SETUP_SCRIPT="${SCRIPT_DIR}/setup_tap.sh"
TEARDOWN_SCRIPT="${SCRIPT_DIR}/teardown_tap.sh"
OUTPUT_DIR="${SIMULATION_DIR}/results/tap_test_$(date +%Y%m%d_%H%M%S)"

# Logs
SIMULATION_LOG="${OUTPUT_DIR}/simulation.log"
AGENT_LOG="${OUTPUT_DIR}/agent.log"
TEST_REPORT="${OUTPUT_DIR}/test_report.txt"

# Exit codes
EXIT_SUCCESS=0
EXIT_TAP_ERROR=1
EXIT_AGENT_ERROR=2
EXIT_RENODE_ERROR=3
EXIT_FIRMWARE_ERROR=4
EXIT_TIMEOUT=5
EXIT_TEST_FAILURE=6

# ========================================
# Helper Functions
# ========================================

usage() {
    cat << EOF
Usage: sudo $(basename "$0") [OPTIONS]

TAP Test Runner for Renode micro-ROS Simulation

Runs complete test of TAP-based networking between Renode simulation
and micro-ROS agent.

Options:
    -i, --interface NAME     TAP interface name (default: ${TAP_INTERFACE})
    -g, --gateway IP         Gateway IP (default: ${GATEWAY_IP})
    -a, --agent-ip IP        Agent IP alias (default: ${AGENT_IP})
    -d, --device-ip IP       Device IP (default: ${DEVICE_IP})
    -p, --port PORT          Agent port (default: ${AGENT_PORT})
    -t, --duration SECONDS   Simulation duration (default: ${SIM_DURATION})
    -n, --no-cleanup         Don't cleanup on exit
    -v, --verbose            Enable verbose output
    -h, --help               Show this help message

Prerequisites:
    - Run with sudo privileges
    - Renode installed at ${RENODE_PATH}
    - Firmware at ${FIRMWARE_PATH}
    - micro-ros-agent available in PATH or Docker

Examples:
    sudo $(basename "$0")
    sudo $(basename "$0") -t 120 -v
    sudo $(basename "$0") --no-cleanup

EOF
}

log() {
    local level="$1"
    local message="$2"
    local timestamp
    timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    case "$level" in
        INFO)  echo -e "\033[0;32m[${timestamp}] [INFO] ${message}\033[0m" ;;
        WARN)  echo -e "\033[0;33m[${timestamp}] [WARN] ${message}\033[0m" ;;
        ERROR) echo -e "\033[0;31m[${timestamp}] [ERROR] ${message}\033[0m" ;;
        DEBUG) [[ "${VERBOSE:-false}" == "true" ]] && echo -e "\033[0;36m[${timestamp}] [DEBUG] ${message}\033[0m" ;;
        *)     echo "[${timestamp}] [${level}] ${message}" ;;
    esac
    
    echo "[$timestamp] [$level] $message" >> "${OUTPUT_DIR}/runner.log"
}

log_info()  { log "INFO" "$1"; }
log_warn()  { log "WARN" "$1"; }
log_error() { log "ERROR" "$1"; }
log_debug() { log "DEBUG" "$1"; }

check_root() {
    if [[ $EUID -ne 0 ]]; then
        echo "ERROR: This script must be run as root (sudo)" >&2
        exit 1
    fi
}

check_dependencies() {
    log_info "Checking dependencies..."
    
    if [[ ! -f "$RENODE_PATH" ]]; then
        log_error "Renode not found at: $RENODE_PATH"
        return $EXIT_RENODE_ERROR
    fi
    log_debug "Renode found"
    
    if [[ ! -f "$FIRMWARE_PATH" ]]; then
        log_error "Firmware not found at: $FIRMWARE_PATH"
        return $EXIT_FIRMWARE_ERROR
    fi
    log_debug "Firmware found"
    
    if [[ ! -f "$TAP_RESC" ]]; then
        log_error "TAP RESC script not found at: $TAP_RESC"
        return $EXIT_RENODE_ERROR
    fi
    log_debug "TAP RESC script found"
    
    if [[ ! -f "$SETUP_SCRIPT" ]]; then
        log_error "Setup script not found at: $SETUP_SCRIPT"
        return $EXIT_TAP_ERROR
    fi
    log_debug "Setup script found"
    
    if [[ ! -f "$TEARDOWN_SCRIPT" ]]; then
        log_error "Teardown script not found at: $TEARDOWN_SCRIPT"
        return $EXIT_TAP_ERROR
    fi
    log_debug "Teardown script found"
    
    log_info "All dependencies satisfied"
    return 0
}

create_output_directory() {
    mkdir -p "$OUTPUT_DIR"
    mkdir -p "${OUTPUT_DIR}/logs"
    mkdir -p "${OUTPUT_DIR}/pcap"
    log_info "Output directory: $OUTPUT_DIR"
}

# ========================================
# TAP Interface Management
# ========================================

setup_tap() {
    log_info "Setting up TAP interface..."
    
    local setup_args=(
        -i "$TAP_INTERFACE"
        -g "$GATEWAY_IP"
        -a "$AGENT_IP"
        -d "$DEVICE_IP"
        -n "$NETMASK"
    )
    
    if [[ "${VERBOSE:-false}" == "true" ]]; then
        setup_args+=(-v)
    fi
    
    if ! bash "$SETUP_SCRIPT" "${setup_args[@]}" 2>&1 | tee -a "${OUTPUT_DIR}/tap_setup.log"; then
        log_error "TAP setup failed"
        return $EXIT_TAP_ERROR
    fi
    
    log_info "TAP interface setup completed"
    return 0
}

teardown_tap() {
    if [[ "${NO_CLEANUP:-false}" == "true" ]]; then
        log_info "Skipping TAP teardown (--no-cleanup)"
        return 0
    fi
    
    log_info "Tearing down TAP interface..."
    
    local teardown_args=(
        -i "$TAP_INTERFACE"
        -a "$AGENT_IP"
        -g "$GATEWAY_IP"
    )
    
    if [[ "${VERBOSE:-false}" == "true" ]]; then
        teardown_args+=(-v)
    fi
    
    bash "$TEARDOWN_SCRIPT" "${teardown_args[@]}" 2>&1 | tee -a "${OUTPUT_DIR}/tap_teardown.log" || true
    
    log_info "TAP interface torn down"
}

# ========================================
# Agent Management
# ========================================

start_agent() {
    log_info "Starting micro-ROS agent..."
    
    local agent_cmd="micro-ros-agent udp4 --port ${AGENT_PORT} -v6"
    
    if command -v micro-ros-agent &> /dev/null; then
        log_debug "Using local micro-ros-agent"
        $agent_cmd > "$AGENT_LOG" 2>&1 &
        AGENT_PID=$!
    elif command -v docker &> /dev/null; then
        log_debug "Using Docker for micro-ros-agent"
        docker run -d --name "microros-agent-tap-$$" \
            -p "${AGENT_PORT}:${AGENT_PORT}/udp" \
            microros/micro-ros-agent:humble \
            udp4 --port "$AGENT_PORT" -v6 > "$AGENT_LOG" 2>&1
        AGENT_DOCKER=true
    else
        log_error "micro-ros-agent not found and Docker not available"
        return $EXIT_AGENT_ERROR
    fi
    
    log_info "Waiting for agent to initialize..."
    sleep "$AGENT_WAIT"
    
    log_info "micro-ROS agent started on ${AGENT_IP}:${AGENT_PORT}"
    return 0
}

stop_agent() {
    if [[ "${NO_CLEANUP:-false}" == "true" ]]; then
        log_info "Skipping agent stop (--no-cleanup)"
        return 0
    fi
    
    log_info "Stopping micro-ROS agent..."
    
    if [[ -n "${AGENT_PID:-}" ]]; then
        kill "$AGENT_PID" 2>/dev/null || true
        wait "$AGENT_PID" 2>/dev/null || true
    fi
    
    if [[ "${AGENT_DOCKER:-false}" == "true" ]]; then
        docker stop "microros-agent-tap-$$" 2>/dev/null || true
        docker rm "microros-agent-tap-$$" 2>/dev/null || true
    fi
    
    log_info "micro-ROS agent stopped"
}

check_agent_activity() {
    log_info "Checking agent logs for XRCE-DDS activity..."
    
    if [[ ! -f "$AGENT_LOG" ]]; then
        log_warn "Agent log not found"
        return 1
    fi
    
    local indicators=("XRCE" "session" "participant" "client" "UDP" "listening")
    local found=()
    
    for indicator in "${indicators[@]}"; do
        if grep -qi "$indicator" "$AGENT_LOG" 2>/dev/null; then
            found+=("$indicator")
        fi
    done
    
    if [[ ${#found[@]} -gt 0 ]]; then
        log_info "Agent activity indicators found: ${found[*]}"
        return 0
    else
        log_warn "No XRCE-DDS activity detected in agent logs"
        return 1
    fi
}

# ========================================
# Renode Simulation
# ========================================

run_simulation() {
    log_info "Starting Renode simulation..."
    log_debug "Duration: ${SIM_DURATION}s"
    
    local renode_cmd=(
        "$RENODE_PATH"
        --disable-xwt
        --console
        "$TAP_RESC"
    )
    
    log_debug "Command: ${renode_cmd[*]}"
    
    local timeout=$((SIM_DURATION + 60))
    local exit_code=0
    
    if ! gtimeout "$timeout" "${renode_cmd[@]}" 2>&1 | tee "$SIMULATION_LOG"; then
        exit_code=$?
        if [[ $exit_code -eq 124 ]]; then
            log_error "Simulation timed out after ${timeout}s"
            return $EXIT_TIMEOUT
        else
            log_error "Simulation failed with exit code: $exit_code"
            return $EXIT_RENODE_ERROR
        fi
    fi
    
    log_info "Simulation completed"
    return 0
}

parse_simulation_log() {
    log_info "Parsing simulation log..."
    
    local boot_success=false
    local uart_activity=false
    local network_activity=false
    local xrcedds_activity=false
    
    if grep -qiE "FreeRTOS|scheduler|started|boot" "$SIMULATION_LOG" 2>/dev/null; then
        boot_success=true
        log_debug "Boot sequence detected"
    fi
    
    if grep -qiE "USART|UART|usart" "$SIMULATION_LOG" 2>/dev/null; then
        uart_activity=true
        log_debug "UART activity detected"
    fi
    
    if grep -qiE "Ethernet|ETH|link|network|UDP|IP" "$SIMULATION_LOG" 2>/dev/null; then
        network_activity=true
        log_debug "Network activity detected"
    fi
    
    if grep -qiE "XRCE|DDS|agent|session|topic|publisher|subscriber" "$SIMULATION_LOG" 2>/dev/null; then
        xrcedds_activity=true
        log_debug "XRCE-DDS activity detected"
    fi
    
    cat > "$TEST_REPORT" << EOF
TAP Networking Test Report
==========================
Generated: $(date)

Network Configuration:
  TAP Interface: ${TAP_INTERFACE}
  Gateway IP:    ${GATEWAY_IP}
  Agent IP:      ${AGENT_IP}:${AGENT_PORT}
  Device IP:     ${DEVICE_IP}
  Netmask:       ${NETMASK}

Test Parameters:
  Simulation Duration: ${SIM_DURATION}s
  Firmware: ${FIRMWARE_PATH}

Results:
  Boot Sequence:   $boot_success
  UART Activity:   $uart_activity
  Network Activity: $network_activity
  XRCE-DDS Activity: $xrcedds_activity

Logs:
  Simulation: ${SIMULATION_LOG}
  Agent: ${AGENT_LOG}
EOF
    
    log_info "Test report saved to: $TEST_REPORT"
}

# ========================================
# Main Test Flow
# ========================================

run_test() {
    local exit_code=0
    
    log_info "================================"
    log_info "TAP Networking Test"
    log_info "================================"
    log_info "Interface: ${TAP_INTERFACE}"
    log_info "Gateway:   ${GATEWAY_IP}"
    log_info "Agent:     ${AGENT_IP}:${AGENT_PORT}"
    log_info "Device:    ${DEVICE_IP}"
    log_info "Duration:  ${SIM_DURATION}s"
    log_info "================================"
    
    create_output_directory
    
    if ! check_dependencies; then
        return $?
    fi
    
    if ! setup_tap; then
        return $?
    fi
    
    if ! start_agent; then
        teardown_tap
        return $?
    fi
    
    if ! run_simulation; then
        exit_code=$?
        stop_agent
        teardown_tap
        return $exit_code
    fi
    
    parse_simulation_log
    
    check_agent_activity || true
    
    stop_agent
    teardown_tap
    
    log_info "================================"
    log_info "Test completed"
    log_info "Report: ${TEST_REPORT}"
    log_info "================================"
    
    return 0
}

# ========================================
# Argument Parsing
# ========================================

VERBOSE="${VERBOSE:-false}"
NO_CLEANUP="${NO_CLEANUP:-false}"

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -i|--interface)
                TAP_INTERFACE="$2"
                shift 2
                ;;
            -g|--gateway)
                GATEWAY_IP="$2"
                shift 2
                ;;
            -a|--agent-ip)
                AGENT_IP="$2"
                shift 2
                ;;
            -d|--device-ip)
                DEVICE_IP="$2"
                shift 2
                ;;
            -p|--port)
                AGENT_PORT="$2"
                shift 2
                ;;
            -t|--duration)
                SIM_DURATION="$2"
                shift 2
                ;;
            -n|--no-cleanup)
                NO_CLEANUP=true
                shift
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
}

cleanup() {
    log_info "Cleanup on exit..."
    stop_agent
    teardown_tap
    pkill -f "renode" || true
}

# ========================================
# Entry Point
# ========================================

main() {
    parse_args "$@"
    check_root
    
    trap cleanup EXIT
    
    if run_test; then
        log_info "All tests passed!"
        exit $EXIT_SUCCESS
    else
        exit_code=$?
        log_error "Test failed with exit code: $exit_code"
        exit $exit_code
    fi
}

main "$@"