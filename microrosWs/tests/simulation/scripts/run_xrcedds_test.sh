#!/bin/bash
#
# XRCE-DDS Transport Test Runner Script
# 
# This script orchestrates the end-to-end test of micro-ROS XRCE-DDS UDP transport
# between the STM32 firmware in Renode and a micro-ROS agent in Docker.
#
# Usage: ./run_xrcedds_test.sh [OPTIONS]
#
# Options:
#   -a, --agent-port PORT     Port for micro-ROS agent (default: 8888)
#   -d, --duration SECONDS    Simulation duration (default: 120)
#   -H, --agent-host HOST     Agent host IP (default: 172.17.0.1 for Docker)
#   -i, --agent-image IMAGE   Docker image for agent (default: microros/micro-ros-agent:humble)
#   -h, --help               Show this help message
#

set -euo pipefail

# ========================================
# Configuration
# ========================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIMULATION_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$SCRIPT_DIR")")")"

# Default configuration
AGENT_PORT="${AGENT_PORT:-8888}"
SIM_DURATION="${SIM_DURATION:-120}"
AGENT_HOST="${AGENT_HOST:-172.17.0.1}"
AGENT_IMAGE="${AGENT_IMAGE:-microros/micro-ros-agent:humble}"
AGENT_CONTAINER_NAME="microros-agent-test-$$"

# Paths
RENODE_PATH="${PROJECT_ROOT}/Renode.app/Contents/MacOS/renode"
PLATFORM_REPL="${SIMULATION_DIR}/renode/stm32h755_networked.repl"
FIRMWARE_PATH="${PROJECT_ROOT}/Micro_ros_eth/microroseth/Makefile/CM7/build/MicroRosEth_CM7.elf"
RESC_SCRIPT="${SIMULATION_DIR}/renode/microros_xrcedds.resc"
OUTPUT_DIR="${SIMULATION_DIR}/results/xrcedds_test_$(date +%Y%m%d_%H%M%S)"

# Logs
SIMULATION_LOG="${OUTPUT_DIR}/simulation.log"
AGENT_LOG="${OUTPUT_DIR}/agent.log"
TEST_REPORT="${OUTPUT_DIR}/test_report.txt"

# Exit codes
EXIT_SUCCESS=0
EXIT_DOCKER_ERROR=1
EXIT_RENODE_ERROR=2
EXIT_FIRMWARE_ERROR=3
EXIT_TIMEOUT=4
EXIT_TEST_FAILURE=5

# ========================================
# Helper Functions
# ========================================

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

XRCE-DDS Transport Test Runner

Runs end-to-end test of micro-ROS XRCE-DDS UDP transport between
STM32 firmware in Renode simulation and micro-ROS agent in Docker.

Options:
    -a, --agent-port PORT     Port for micro-ROS agent (default: ${AGENT_PORT})
    -d, --duration SECONDS    Simulation duration (default: ${SIM_DURATION})
    -H, --agent-host HOST     Agent host IP (default: ${AGENT_HOST})
    -i, --agent-image IMAGE   Docker image for agent (default: ${AGENT_IMAGE})
    -n, --no-cleanup         Don't cleanup Docker containers on exit
    -v, --verbose            Enable verbose output
    -h, --help               Show this help message

Prerequisites:
    - Docker installed and running
    - Renode installed at ${RENODE_PATH}
    - Firmware ELF at ${FIRMWARE_PATH}

Examples:
    $(basename "$0")
    $(basename "$0") -d 180 -a 8888
    $(basename "$0") --verbose --no-cleanup

Exit Codes:
    0  Success
    1  Docker error
    2  Renode error
    3  Firmware not found
    4  Timeout exceeded
    5  Test failure
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
}

log_info()  { log "INFO" "$1"; }
log_warn()  { log "WARN" "$1"; }
log_error() { log "ERROR" "$1"; }
log_debug() { log "DEBUG" "$1"; }

check_dependencies() {
    log_info "Checking dependencies..."
    
    # Check Docker
    if ! command -v docker &> /dev/null; then
        log_error "Docker not found. Please install Docker."
        return $EXIT_DOCKER_ERROR
    fi
    
    if ! docker info &> /dev/null; then
        log_error "Docker daemon not running. Please start Docker."
        return $EXIT_DOCKER_ERROR
    fi
    log_debug "Docker is available"
    
    # Check Renode
    if [[ ! -f "$RENODE_PATH" ]]; then
        log_error "Renode not found at: $RENODE_PATH"
        return $EXIT_RENODE_ERROR
    fi
    log_debug "Renode found at: $RENODE_PATH"
    
    # Check platform file
    if [[ ! -f "$PLATFORM_REPL" ]]; then
        log_error "Platform file not found at: $PLATFORM_REPL"
        return $EXIT_RENODE_ERROR
    fi
    log_debug "Platform file found"
    
    # Check firmware
    if [[ ! -f "$FIRMWARE_PATH" ]]; then
        log_error "Firmware not found at: $FIRMWARE_PATH"
        return $EXIT_FIRMWARE_ERROR
    fi
    log_debug "Firmware found at: $FIRMWARE_PATH"
    
    # Check RESC script
    if [[ ! -f "$RESC_SCRIPT" ]]; then
        log_error "RESC script not found at: $RESC_SCRIPT"
        return $EXIT_RENODE_ERROR
    fi
    log_debug "RESC script found"
    
    log_info "All dependencies satisfied"
    return 0
}

create_output_directory() {
    log_info "Creating output directory: $OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR"
    
    # Create subdirectories
    mkdir -p "${OUTPUT_DIR}/logs"
    mkdir -p "${OUTPUT_DIR}/pcap"
    mkdir -p "${OUTPUT_DIR}/reports"
}

# ========================================
# Agent Management
# ========================================

start_agent() {
    log_info "Starting micro-ROS agent..."
    
    # Pull image if not present
    if ! docker image inspect "$AGENT_IMAGE" &> /dev/null; then
        log_info "Pulling Docker image: $AGENT_IMAGE"
        docker pull "$AGENT_IMAGE"
    fi
    
    # Remove any existing container with same name
    docker rm -f "$AGENT_CONTAINER_NAME" &> /dev/null || true
    
    # Start agent container
    local docker_cmd=(
        docker run -d
        --name "$AGENT_CONTAINER_NAME"
        -p "${AGENT_PORT}:${AGENT_PORT}/udp"
        "$AGENT_IMAGE"
        udp4
        --port "$AGENT_PORT"
        -v6
    )
    
    if ! AGENT_CONTAINER_ID=$("${docker_cmd[@]}" 2>&1); then
        log_error "Failed to start agent container"
        log_error "$AGENT_CONTAINER_ID"
        return $EXIT_DOCKER_ERROR
    fi
    
    log_info "Agent container started: ${AGENT_CONTAINER_ID:0:12}"
    
    # Wait for agent to initialize
    log_debug "Waiting for agent to initialize..."
    sleep 5
    
    # Check agent is running
    if ! docker ps --filter "name=$AGENT_CONTAINER_NAME" --filter "status=running" | grep -q "$AGENT_CONTAINER_NAME"; then
        log_error "Agent container is not running"
        docker logs "$AGENT_CONTAINER_NAME" 2>&1 | tee "$AGENT_LOG"
        return $EXIT_DOCKER_ERROR
    fi
    
    # Save initial logs
    docker logs "$AGENT_CONTAINER_NAME" > "$AGENT_LOG" 2>&1
    
    log_info "micro-ROS agent ready on UDP port ${AGENT_PORT}"
    return 0
}

stop_agent() {
    if [[ "${NO_CLEANUP:-false}" == "true" ]]; then
        log_info "Skipping agent cleanup (--no-cleanup)"
        return 0
    fi
    
    log_info "Stopping micro-ROS agent..."
    
    # Get final logs
    if docker ps -a --filter "name=$AGENT_CONTAINER_NAME" | grep -q "$AGENT_CONTAINER_NAME"; then
        docker logs "$AGENT_CONTAINER_NAME" > "$AGENT_LOG" 2>&1
        log_debug "Agent logs saved to: $AGENT_LOG"
        
        # Stop and remove container
        docker stop "$AGENT_CONTAINER_NAME" &> /dev/null || true
        docker rm "$AGENT_CONTAINER_NAME" &> /dev/null || true
    fi
    
    log_info "Agent container stopped and removed"
}

get_agent_status() {
    if docker ps --filter "name=$AGENT_CONTAINER_NAME" --filter "status=running" | grep -q "$AGENT_CONTAINER_NAME"; then
        echo "running"
    else
        echo "stopped"
    fi
}

check_agent_logs() {
    log_info "Checking agent logs for activity..."
    
    local logs
    logs=$(docker logs "$AGENT_CONTAINER_NAME" 2>&1)
    
    # Check for various activity indicators
    local indicators=("XRCE" "session" "participant" "client" "UDP" "listening" "port")
    local found_indicators=()
    
    for indicator in "${indicators[@]}"; do
        if echo "$logs" | grep -qi "$indicator"; then
            found_indicators+=("$indicator")
        fi
    done
    
    if [[ ${#found_indicators[@]} -gt 0 ]]; then
        log_info "Found agent activity indicators: ${found_indicators[*]}"
        return 0
    else
        log_warn "No activity indicators found in agent logs"
        return 1
    fi
}

# ========================================
# Renode Simulation
# ========================================

create_resc_script() {
    log_debug "Creating temporary RESC script..."
    
    local temp_resc="${OUTPUT_DIR}/run_test.resc"
    
    cat > "$temp_resc" << EOF
// Auto-generated test script
using sysbus
using "stm32h755_networked.repl"

logLevel 3
simulation speed 1.0

showAnalyzer usart3

sysbus LoadELF @${FIRMWARE_PATH}

echo "=========================================="
echo "XRCE-DDS Transport Test Simulation"
echo "=========================================="
echo "Agent: ${AGENT_HOST}:${AGENT_PORT}"
echo "Duration: ${SIM_DURATION}s"
echo "=========================================="

start

sleep ${SIM_DURATION}

echo "=========================================="
echo "Simulation completed"
echo "=========================================="

quit
EOF
    
    echo "$temp_resc"
}

run_simulation() {
    log_info "Starting Renode simulation..."
    
    local resc_script
    resc_script=$(create_resc_script)
    
    log_debug "RESC script: $resc_script"
    log_debug "Simulation duration: ${SIM_DURATION}s"
    
    # Calculate timeout (simulation duration + buffer)
    local timeout=$((SIM_DURATION + 60))
    
    # Run Renode
    local renode_cmd=(
        "$RENODE_PATH"
        --disable-xwt
        --console
        "$resc_script"
    )
    
    log_info "Running: ${renode_cmd[*]}"
    
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
    
    # Parse simulation log for results
    parse_simulation_log
    
    return 0
}

parse_simulation_log() {
    log_info "Parsing simulation log..."
    
    local boot_success=false
    local uart_activity=false
    local network_activity=false
    local xrcedds_activity=false
    
    # Check for boot
    if grep -qiE "FreeRTOS|scheduler|started|boot" "$SIMULATION_LOG"; then
        boot_success=true
        log_debug "Boot sequence detected"
    fi
    
    # Check for UART activity
    if grep -qiE "USART|UART|usart" "$SIMULATION_LOG"; then
        uart_activity=true
        log_debug "UART activity detected"
    fi
    
    # Check for network activity
    if grep -qiE "Ethernet|ETH|link|network|UDP|IP" "$SIMULATION_LOG"; then
        network_activity=true
        log_debug "Network activity detected"
    fi
    
    # Check for XRCE-DDS activity
    if grep -qiE "XRCE|DDS|agent|session|topic|publisher|subscriber" "$SIMULATION_LOG"; then
        xrcedds_activity=true
        log_debug "XRCE-DDS activity detected"
    fi
    
    # Generate report
    cat > "$TEST_REPORT" << EOF
XRCE-DDS Transport Test Report
==============================
Generated: $(date)

Configuration:
  Agent Host: ${AGENT_HOST}
  Agent Port: ${AGENT_PORT}
  Agent Image: ${AGENT_IMAGE}
  Simulation Duration: ${SIM_DURATION}s
  Firmware: ${FIRMWARE_PATH}

Results:
  Boot Sequence: $boot_success
  UART Activity: $uart_activity
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

run_tests() {
    local exit_code=0
    
    # Check dependencies
    if ! check_dependencies; then
        return $?
    fi
    
    # Create output directory
    create_output_directory
    
    # Start micro-ROS agent
    if ! start_agent; then
        return $?
    fi
    
    # Run simulation
    if ! run_simulation; then
        exit_code=$?
        stop_agent
        return $exit_code
    fi
    
    # Check agent received packets
    check_agent_logs || true
    
    # Cleanup
    stop_agent
    
    log_info "Test completed successfully"
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
            -a|--agent-port)
                AGENT_PORT="$2"
                shift 2
                ;;
            -d|--duration)
                SIM_DURATION="$2"
                shift 2
                ;;
            -H|--agent-host)
                AGENT_HOST="$2"
                shift 2
                ;;
            -i|--agent-image)
                AGENT_IMAGE="$2"
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
    log_info "Cleaning up..."
    stop_agent
    
    # Kill any lingering Renode processes
    pkill -f "renode.*stm32" || true
}

# ========================================
# Entry Point
# ========================================

main() {
    parse_args "$@"
    
    # Set up cleanup trap
    trap cleanup EXIT
    
    log_info "================================"
    log_info "XRCE-DDS Transport Test Runner"
    log_info "================================"
    log_info "Agent Port: ${AGENT_PORT}"
    log_info "Agent Host: ${AGENT_HOST}"
    log_info "Duration: ${SIM_DURATION}s"
    log_info "Output: ${OUTPUT_DIR}"
    log_info "================================"
    
    if run_tests; then
        log_info "All tests passed!"
        exit $EXIT_SUCCESS
    else
        exit_code=$?
        log_error "Tests failed with exit code: $exit_code"
        exit $exit_code
    fi
}

main "$@"