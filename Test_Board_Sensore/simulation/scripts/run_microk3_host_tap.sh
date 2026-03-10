#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MICROK3_DIR="$ROOT_DIR/microrosWs/microk3"
RENODE_SCRIPT="$ROOT_DIR/Test_Board_Sensore/simulation/scripts/microroseth_validation_tap.resc"
RENODE_BIN_DEFAULT="$ROOT_DIR/renode/output/bin/Release/Renode"

TAP_INTERFACE="${TAP_INTERFACE:-tap0}"
HOST_TAP_IP="${HOST_TAP_IP:-192.168.50.1}"
HOST_NETMASK="${HOST_NETMASK:-255.255.255.0}"
RENODE_PATH="${RENODE_PATH:-$RENODE_BIN_DEFAULT}"

setup_tap_linux() {
  sudo ip tuntap add dev "$TAP_INTERFACE" mode tap 2>/dev/null || true
  sudo ip addr flush dev "$TAP_INTERFACE" 2>/dev/null || true
  sudo ip addr add "${HOST_TAP_IP}/24" dev "$TAP_INTERFACE"
  sudo ip link set "$TAP_INTERFACE" up
}

setup_tap_macos() {
  if [[ ! -e "/dev/${TAP_INTERFACE}" ]]; then
    echo "Missing /dev/${TAP_INTERFACE}. Install TAP support first (for example tuntaposx)." >&2
    exit 1
  fi

  sudo ifconfig "$TAP_INTERFACE" "$HOST_TAP_IP" netmask "$HOST_NETMASK" up
}

start_stack() {
  (cd "$MICROK3_DIR" && docker compose up -d uros-agent microk3 renode-bridge)
}

run_renode() {
  "$RENODE_PATH" --disable-gui --plain "$RENODE_SCRIPT"
}

case "$(uname -s)" in
  Linux)
    setup_tap_linux
    ;;
  Darwin)
    setup_tap_macos
    ;;
  *)
    echo "Unsupported host OS: $(uname -s)" >&2
    exit 1
    ;;
esac

start_stack

echo "Host TAP ready on ${TAP_INTERFACE} (${HOST_TAP_IP})"
echo "microk3 stack started from ${MICROK3_DIR}"
echo "Launching Renode script ${RENODE_SCRIPT}"

run_renode
