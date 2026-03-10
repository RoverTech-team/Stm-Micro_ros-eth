#!/bin/bash
#
# TAP Interface Setup Script for Renode micro-ROS Simulation
#
# Creates a TAP interface for communication between Renode simulation
# and micro-ROS agent. The setup creates:
#   - TAP interface (tap0) with IP 192.168.0.1 (gateway)
#   - IP alias 192.168.0.8 for micro-ROS agent binding
#
# Network topology:
#   STM32 (192.168.0.3) <---> tap0 (192.168.0.1) <---> Agent (192.168.0.8:8888)
#
# Usage: sudo ./setup_tap.sh [OPTIONS]
#
# Options:
#   -i, --interface NAME    TAP interface name (default: tap0)
#   -g, --gateway IP        Gateway IP address (default: 192.168.0.1)
#   -a, --agent-ip IP       Agent IP alias (default: 192.168.0.8)
#   -n, --netmask MASK      Netmask (default: 255.255.255.0)
#   -h, --help              Show this help message
#

set -euo pipefail

TAP_INTERFACE="${TAP_INTERFACE:-tap0}"
GATEWAY_IP="${GATEWAY_IP:-192.168.0.1}"
AGENT_IP="${AGENT_IP:-192.168.0.8}"
NETMASK="${NETMASK:-255.255.255.0}"
DEVICE_IP="${DEVICE_IP:-192.168.0.3}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="/tmp/tap_setup_$(date +%Y%m%d_%H%M%S).log"

usage() {
    cat << EOF
Usage: sudo $(basename "$0") [OPTIONS]

TAP Interface Setup for Renode micro-ROS Simulation

Creates TAP interface for communication between Renode simulation
and micro-ROS agent.

Options:
    -i, --interface NAME    TAP interface name (default: ${TAP_INTERFACE})
    -g, --gateway IP        Gateway IP address (default: ${GATEWAY_IP})
    -a, --agent-ip IP       Agent IP alias (default: ${AGENT_IP})
    -n, --netmask MASK      Netmask (default: ${NETMASK})
    -d, --device-ip IP      STM32 device IP (default: ${DEVICE_IP})
    -v, --verbose           Enable verbose output
    -h, --help              Show this help message

Network Topology:
    STM32 (${DEVICE_IP}) <---> ${TAP_INTERFACE} (${GATEWAY_IP}) <---> Agent (${AGENT_IP}:8888)

Prerequisites:
    - Run with sudo privileges
    - tuntap kernel module available (macOS has built-in utun)

Examples:
    sudo $(basename "$0")
    sudo $(basename "$0") -i tap1 -g 10.0.0.1 -a 10.0.0.10

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

check_os() {
    case "$(uname -s)" in
        Darwin)
            OS="macos"
            log_info "Detected macOS"
            ;;
        Linux)
            OS="linux"
            log_info "Detected Linux"
            ;;
        *)
            log_error "Unsupported OS: $(uname -s)"
            exit 1
            ;;
    esac
}

check_existing_tap() {
    if ip link show "$TAP_INTERFACE" &> /dev/null 2>&1; then
        log_warn "TAP interface '$TAP_INTERFACE' already exists"
        return 0
    fi
    return 1
}

check_macos_tap_support() {
    local tap_device="/dev/${TAP_INTERFACE}"
    local kext_loaded=false
    local brew_installed=false
    
    log_debug "Checking for TAP device: $tap_device"
    if [[ -e "$tap_device" ]]; then
        log_debug "TAP device exists: $tap_device"
        return 0
    fi
    
    log_debug "Checking for tuntaposx kernel extension..."
    if kextstat 2>/dev/null | grep -qE "(com.apple.driver.tun|net.sf.tuntaposx.tap|foo.tun|org.macforge.tuntap)"; then
        kext_loaded=true
        log_debug "Tuntap kernel extension found in kextstat"
    fi
    
    log_debug "Checking for tuntap via Homebrew..."
    if brew list tuntap &>/dev/null 2>&1; then
        brew_installed=true
        log_debug "tuntap is installed via Homebrew"
    fi
    
    if [[ "$kext_loaded" == "true" ]]; then
        log_info "Tuntap kernel extension is loaded"
        return 0
    fi
    
    if [[ "$brew_installed" == "true" ]]; then
        log_warn "tuntap is installed via Homebrew but kernel extension not loaded"
        log_info "Attempting to load kernel extension..."
        
        if [[ -f "/Library/Extensions/tap.kext" ]]; then
            if kextload "/Library/Extensions/tap.kext" 2>/dev/null; then
                log_info "Kernel extension loaded successfully"
                return 0
            else
                log_error "Failed to load kernel extension"
            fi
        elif [[ -f "/Library/Extensions/tuntap.kext" ]]; then
            if kextload "/Library/Extensions/tuntap.kext" 2>/dev/null; then
                log_info "Kernel extension loaded successfully"
                return 0
            else
                log_error "Failed to load kernel extension"
            fi
        fi
    fi
    
    log_error "TAP interface support not available on macOS"
    log_info "To enable TAP support, install tuntaposx:"
    log_info "  brew install tuntap"
    log_info "  sudo kextload /Library/Extensions/tap.kext"
    log_warn "Note: tuntaposx may not work on macOS 12+ (Monterey) due to security restrictions"
    log_info "Alternative: Run tests on Linux VM or in Docker with --privileged"
    
    return 1
}

setup_tap_macos() {
    log_info "Setting up TAP interface on macOS..."
    
    if ! check_macos_tap_support; then
        log_error "TAP support check failed. Cannot proceed with setup."
        exit 1
    fi
    
    if [[ ! -e "/dev/$TAP_INTERFACE" ]]; then
        log_info "Creating TAP device: $TAP_INTERFACE"
        
        if ! mknod "/dev/$TAP_INTERFACE" c 10 200 2>/dev/null; then
            log_debug "TAP device node creation failed or already exists"
        fi
    fi
    
    ifconfig "$TAP_INTERFACE" &> /dev/null 2>&1 || true
    
    log_info "Configuring $TAP_INTERFACE with IP $GATEWAY_IP"
    ifconfig "$TAP_INTERFACE" "$GATEWAY_IP" netmask "$NETMASK" up
    
    log_info "Adding IP alias $AGENT_IP to $TAP_INTERFACE"
    ifconfig "$TAP_INTERFACE" alias "$AGENT_IP" netmask "$NETMASK"
    
    log_info "TAP interface configured successfully"
    show_interface_status
}

setup_tap_linux() {
    log_info "Setting up TAP interface on Linux..."
    
    if ! command -v ip &> /dev/null; then
        log_error "ip command not found"
        exit 1
    fi
    
    if check_existing_tap; then
        log_info "Removing existing TAP interface"
        ip link delete "$TAP_INTERFACE" 2>/dev/null || true
    fi
    
    log_info "Creating TAP device: $TAP_INTERFACE"
    ip tuntap add dev "$TAP_INTERFACE" mode tap
    
    log_info "Configuring $TAP_INTERFACE with IP $GATEWAY_IP"
    ip addr add "$GATEWAY_IP/24" dev "$TAP_INTERFACE"
    
    log_info "Adding IP alias $AGENT_IP to $TAP_INTERFACE"
    ip addr add "$AGENT_IP/24" dev "$TAP_INTERFACE"
    
    log_info "Bringing up $TAP_INTERFACE"
    ip link set "$TAP_INTERFACE" up
    
    log_info "TAP interface configured successfully"
    show_interface_status
}

show_interface_status() {
    log_info "=========================================="
    log_info "TAP Interface Status"
    log_info "=========================================="
    
    case "$OS" in
        macos)
            ifconfig "$TAP_INTERFACE" 2>/dev/null | while read -r line; do
                log_info "$line"
            done
            ;;
        linux)
            ip addr show "$TAP_INTERFACE" 2>/dev/null | while read -r line; do
                log_info "$line"
            done
            ;;
    esac
    
    log_info "=========================================="
    log_info "Network Configuration Summary:"
    log_info "  TAP Interface: $TAP_INTERFACE"
    log_info "  Gateway IP:    $GATEWAY_IP"
    log_info "  Agent IP:      $AGENT_IP"
    log_info "  Device IP:     $DEVICE_IP"
    log_info "  Netmask:       $NETMASK"
    log_info "=========================================="
}

verify_connectivity() {
    log_info "Verifying interface configuration..."
    
    case "$OS" in
        macos)
            if ifconfig "$TAP_INTERFACE" | grep -q "inet $GATEWAY_IP"; then
                log_info "Gateway IP ($GATEWAY_IP) configured correctly"
            else
                log_error "Gateway IP not configured"
                return 1
            fi
            
            if ifconfig "$TAP_INTERFACE" | grep -q "inet $AGENT_IP"; then
                log_info "Agent IP alias ($AGENT_IP) configured correctly"
            else
                log_error "Agent IP alias not configured"
                return 1
            fi
            ;;
        linux)
            if ip addr show "$TAP_INTERFACE" | grep -q "$GATEWAY_IP"; then
                log_info "Gateway IP ($GATEWAY_IP) configured correctly"
            else
                log_error "Gateway IP not configured"
                return 1
            fi
            
            if ip addr show "$TAP_INTERFACE" | grep -q "$AGENT_IP"; then
                log_info "Agent IP alias ($AGENT_IP) configured correctly"
            else
                log_error "Agent IP alias not configured"
                return 1
            fi
            ;;
    esac
    
    log_info "Interface verification passed"
    return 0
}

enable_forwarding() {
    log_info "Enabling IP forwarding..."
    
    case "$OS" in
        macos)
            sysctl -w net.inet.ip.forwarding=1
            ;;
        linux)
            echo 1 > /proc/sys/net/ipv4/ip_forward
            ;;
    esac
    
    log_info "IP forwarding enabled"
}

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
            -n|--netmask)
                NETMASK="$2"
                shift 2
                ;;
            -d|--device-ip)
                DEVICE_IP="$2"
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
    log_info "TAP Interface Setup for Renode Simulation"
    log_info "=========================================="
    
    check_root
    check_os
    
    case "$OS" in
        macos)
            setup_tap_macos
            ;;
        linux)
            setup_tap_linux
            ;;
    esac
    
    verify_connectivity
    enable_forwarding
    
    log_info "=========================================="
    log_info "TAP setup completed successfully"
    log_info "Log file: $LOG_FILE"
    log_info "=========================================="
    
    echo ""
    echo "TAP interface ready for Renode simulation"
    echo ""
    echo "Network topology:"
    echo "  STM32 Device: ${DEVICE_IP}"
    echo "  Gateway (TAP): ${GATEWAY_IP}"
    echo "  Agent (alias): ${AGENT_IP}:8888"
    echo ""
    echo "Next steps:"
    echo "  1. Start micro-ROS agent: micro-ros-agent udp4 --port 8888 -v6"
    echo "  2. Run Renode with TAP platform"
    echo ""
}

main "$@"
