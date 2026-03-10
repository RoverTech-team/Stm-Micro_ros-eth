#!/bin/bash
#
# TAP Interface Teardown Script for Renode micro-ROS Simulation
#
# Cleans up TAP interface and IP aliases created by setup_tap.sh
#
# Usage: sudo ./teardown_tap.sh [OPTIONS]
#
# Options:
#   -i, --interface NAME    TAP interface name (default: tap0)
#   -a, --agent-ip IP       Agent IP alias to remove (default: 192.168.0.8)
#   -g, --gateway IP        Gateway IP to remove (default: 192.168.0.1)
#   -h, --help              Show this help message
#

set -euo pipefail

TAP_INTERFACE="${TAP_INTERFACE:-tap0}"
GATEWAY_IP="${GATEWAY_IP:-192.168.0.1}"
AGENT_IP="${AGENT_IP:-192.168.0.8}"
NETMASK="${NETMASK:-255.255.255.0}"

LOG_FILE="/tmp/tap_teardown_$(date +%Y%m%d_%H%M%S).log"

usage() {
    cat << EOF
Usage: sudo $(basename "$0") [OPTIONS]

TAP Interface Teardown for Renode micro-ROS Simulation

Cleans up TAP interface and associated IP configuration.

Options:
    -i, --interface NAME    TAP interface name (default: ${TAP_INTERFACE})
    -a, --agent-ip IP       Agent IP alias to remove (default: ${AGENT_IP})
    -g, --gateway IP        Gateway IP to remove (default: ${GATEWAY_IP})
    -v, --verbose           Enable verbose output
    -h, --help              Show this help message

Examples:
    sudo $(basename "$0")
    sudo $(basename "$0") -i tap1

EOF
}

log() {
    local level="$1"
    local message="$2"
    local timestamp
    timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    echo "[$timestamp] [$level] $message" | tee -a "$LOG_FILE"
}

log_info()  { log "INFO" "$1"; }
log_warn()  { log "WARN" "$1"; }
log_error() { log "ERROR" "$1"; }
log_debug() { [[ "${VERBOSE:-false}" == "true" ]] && log "DEBUG" "$1" || true; }

check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (sudo)"
        exit 1
    fi
}

detect_os() {
    case "$(uname -s)" in
        Darwin) OS="macos" ;;
        Linux)  OS="linux" ;;
        *)
            log_error "Unsupported OS: $(uname -s)"
            exit 1
            ;;
    esac
}

teardown_tap_macos() {
    log_info "Tearing down TAP interface on macOS..."
    
    if ! ifconfig "$TAP_INTERFACE" &> /dev/null; then
        log_warn "TAP interface '$TAP_INTERFACE' does not exist"
        return 0
    fi
    
    log_info "Removing IP alias $AGENT_IP from $TAP_INTERFACE"
    ifconfig "$TAP_INTERFACE" inet "$AGENT_IP" -alias 2>/dev/null || log_debug "Alias already removed"
    
    log_info "Removing IP address $GATEWAY_IP from $TAP_INTERFACE"
    ifconfig "$TAP_INTERFACE" inet "$GATEWAY_IP" -alias 2>/dev/null || log_debug "IP already removed"
    
    log_info "Bringing down $TAP_INTERFACE"
    ifconfig "$TAP_INTERFACE" down 2>/dev/null || true
    
    log_info "TAP interface torn down successfully"
}

teardown_tap_linux() {
    log_info "Tearing down TAP interface on Linux..."
    
    if ! ip link show "$TAP_INTERFACE" &> /dev/null 2>&1; then
        log_warn "TAP interface '$TAP_INTERFACE' does not exist"
        return 0
    fi
    
    log_info "Bringing down $TAP_INTERFACE"
    ip link set "$TAP_INTERFACE" down 2>/dev/null || true
    
    log_info "Deleting TAP interface $TAP_INTERFACE"
    ip tuntap delete dev "$TAP_INTERFACE" mode tap 2>/dev/null || true
    
    log_info "TAP interface torn down successfully"
}

disable_forwarding() {
    log_info "Disabling IP forwarding..."
    
    case "$OS" in
        macos)
            sysctl -w net.inet.ip.forwarding=0 2>/dev/null || true
            ;;
        linux)
            echo 0 > /proc/sys/net/ipv4/ip_forward 2>/dev/null || true
            ;;
    esac
    
    log_info "IP forwarding disabled"
}

kill_agent_processes() {
    log_info "Checking for micro-ROS agent processes..."
    
    local agent_pids
    agent_pids=$(pgrep -f "micro-ros-agent" 2>/dev/null || true)
    
    if [[ -n "$agent_pids" ]]; then
        log_info "Found micro-ROS agent processes: $agent_pids"
        log_info "Stopping agent processes..."
        echo "$agent_pids" | xargs kill 2>/dev/null || true
        sleep 1
        
        agent_pids=$(pgrep -f "micro-ros-agent" 2>/dev/null || true)
        if [[ -n "$agent_pids" ]]; then
            log_warn "Force killing agent processes..."
            echo "$agent_pids" | xargs kill -9 2>/dev/null || true
        fi
    else
        log_info "No micro-ROS agent processes found"
    fi
}

kill_renode_processes() {
    log_info "Checking for Renode processes..."
    
    local renode_pids
    renode_pids=$(pgrep -f "renode" 2>/dev/null || true)
    
    if [[ -n "$renode_pids" ]]; then
        log_info "Found Renode processes: $renode_pids"
        log_info "Stopping Renode processes..."
        echo "$renode_pids" | xargs kill 2>/dev/null || true
        sleep 1
    else
        log_info "No Renode processes found"
    fi
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -i|--interface)
                TAP_INTERFACE="$2"
                shift 2
                ;;
            -a|--agent-ip)
                AGENT_IP="$2"
                shift 2
                ;;
            -g|--gateway)
                GATEWAY_IP="$2"
                shift 2
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

main() {
    parse_args "$@"
    
    log_info "=========================================="
    log_info "TAP Interface Teardown"
    log_info "=========================================="
    
    check_root
    detect_os
    
    kill_agent_processes
    kill_renode_processes
    
    case "$OS" in
        macos)
            teardown_tap_macos
            ;;
        linux)
            teardown_tap_linux
            ;;
    esac
    
    disable_forwarding
    
    log_info "=========================================="
    log_info "TAP teardown completed"
    log_info "Log file: $LOG_FILE"
    log_info "=========================================="
}

main "$@"
