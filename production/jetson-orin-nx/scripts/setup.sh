#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
. "$SCRIPT_DIR/load_env.sh"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
load_env_defaults "$ROOT_DIR/.env"
STACK="${STACK:-renode}"
COMPOSE_FILE="$("$SCRIPT_DIR/resolve-compose.sh")"
. "$SCRIPT_DIR/stm32_programmer_env.sh"
. "$SCRIPT_DIR/ip_env.sh"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is not installed or not in PATH."
  exit 1
fi

if ! docker compose version >/dev/null 2>&1; then
  echo "docker compose plugin is not available."
  exit 1
fi

case "$STACK" in
  hil-macos)
    WORKSPACE_DIR="$(cd "$ROOT_DIR/../.." && pwd)"
    MAC_HOST_IP="${MAC_HOST_IP:-192.168.2.1}"
    STM_BOARD_IP="${STM_BOARD_IP:-192.168.2.2}"
    CM4_IMAGE="${CM4_IMAGE:-}"
    CM4_MAKE_DIR="${CM4_MAKE_DIR:-}"
    CM7_IMAGE="${CM7_IMAGE:-}"
    CM7_MAKE_DIR="${CM7_MAKE_DIR:-}"
    setup_stm32_programmer_path
    STM32_PROGRAMMER_CLI="${STM32_PROGRAMMER_CLI:-$(resolve_stm32_programmer_cli || true)}"

    require_ipv4 "MAC_HOST_IP" "$MAC_HOST_IP"
    require_ipv4 "STM_BOARD_IP" "$STM_BOARD_IP"

    if ! docker info >/dev/null 2>&1; then
      echo "Docker Desktop is not reachable. Start Docker Desktop and try again."
      exit 1
    fi

    if [ ! -x "${STM32_PROGRAMMER_CLI}" ]; then
      echo "STM32_Programmer_CLI was not found in PATH or on /Volumes/Untitled."
      exit 1
    fi

    if [ -z "$CM4_IMAGE" ] || [ -z "$CM4_MAKE_DIR" ] || [ -z "$CM7_IMAGE" ] || [ -z "$CM7_MAKE_DIR" ]; then
      echo "CM4_IMAGE, CM4_MAKE_DIR, CM7_IMAGE, and CM7_MAKE_DIR must be set in .env or the shell for STACK=hil-macos."
      exit 1
    fi

    if [ ! -f "$CM4_IMAGE" ]; then
      echo "CM4 image not found at $CM4_IMAGE."
      exit 1
    fi

    if [ ! -f "$CM4_MAKE_DIR/Makefile" ]; then
      echo "CM4 Makefile not found at $CM4_MAKE_DIR."
      exit 1
    fi

    if [ ! -f "$CM7_MAKE_DIR/Makefile" ]; then
      echo "CM7 HIL Makefile not found at $CM7_MAKE_DIR."
      exit 1
    fi

    if [ ! -f "$CM7_IMAGE" ]; then
      echo "CM7 image not found at $CM7_IMAGE."
      exit 1
    fi
    ;;
  hil|renode|*)
    if [ ! -c /dev/net/tun ]; then
      echo "/dev/net/tun is missing. Ensure TUN/TAP is enabled in the kernel."
      exit 1
    fi

    if ! id -nG "$(id -un)" | tr ' ' '\n' | grep -q '^docker$'; then
      echo "User is not in the docker group. Use sudo or add the user to the docker group."
    fi
    ;;
esac

if [ ! -f "$COMPOSE_FILE" ]; then
  echo "Compose file not found at $COMPOSE_FILE."
  exit 1
fi

echo "Setup checks passed."
