#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/load_env.sh"
load_env_defaults "$ROOT_DIR/.env"
. "$SCRIPT_DIR/stm32_programmer_env.sh"
. "$SCRIPT_DIR/ip_env.sh"

setup_stm32_programmer_path
STM32_PROGRAMMER_CLI="${STM32_PROGRAMMER_CLI:-$(resolve_stm32_programmer_cli || true)}"
STM32_PROGRAMMER_PORT="${STM32_PROGRAMMER_PORT:-SWD}"
MAC_HOST_IP="${MAC_HOST_IP:-192.168.2.1}"
STM_BOARD_IP="${STM_BOARD_IP:-192.168.2.2}"
CM4_IMAGE="${CM4_IMAGE:-}"
CM4_MAKE_DIR="${CM4_MAKE_DIR:-}"
CM7_IMAGE="${CM7_IMAGE:-}"
CM7_MAKE_DIR="${CM7_MAKE_DIR:-}"

if [ ! -x "$STM32_PROGRAMMER_CLI" ]; then
  echo "STM32_Programmer_CLI was not found in PATH or on /Volumes/Untitled."
  exit 1
fi

require_ipv4 "MAC_HOST_IP" "$MAC_HOST_IP"
require_ipv4 "STM_BOARD_IP" "$STM_BOARD_IP"

if [ -z "$CM4_IMAGE" ] || [ -z "$CM4_MAKE_DIR" ] || [ -z "$CM7_IMAGE" ] || [ -z "$CM7_MAKE_DIR" ]; then
  echo "CM4_IMAGE, CM4_MAKE_DIR, CM7_IMAGE, and CM7_MAKE_DIR must be set in .env or the shell."
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

set -- $(split_ipv4 "$MAC_HOST_IP")
MAC_IP_A="$1"
MAC_IP_B="$2"
MAC_IP_C="$3"
MAC_IP_D="$4"

set -- $(split_ipv4 "$STM_BOARD_IP")
STM_IP_A="$1"
STM_IP_B="$2"
STM_IP_C="$3"
STM_IP_D="$4"

echo "Building CM4 for stm=$STM_BOARD_IP"
make -C "$CM4_MAKE_DIR" clean
make -C "$CM4_MAKE_DIR" -j"$(sysctl -n hw.ncpu)"

echo "Building CM7 HIL for stm=$STM_BOARD_IP agent=$MAC_HOST_IP"
make -C "$CM7_MAKE_DIR" clean
make -C "$CM7_MAKE_DIR" -j"$(sysctl -n hw.ncpu)" \
  MICROROS_AGENT_IP="$MAC_HOST_IP" \
  MICROROS_DEVICE_IP_A="$STM_IP_A" \
  MICROROS_DEVICE_IP_B="$STM_IP_B" \
  MICROROS_DEVICE_IP_C="$STM_IP_C" \
  MICROROS_DEVICE_IP_D="$STM_IP_D" \
  MICROROS_GATEWAY_IP_A="$MAC_IP_A" \
  MICROROS_GATEWAY_IP_B="$MAC_IP_B" \
  MICROROS_GATEWAY_IP_C="$MAC_IP_C" \
  MICROROS_GATEWAY_IP_D="$MAC_IP_D"

if [ ! -f "$CM7_IMAGE" ]; then
  echo "CM7 image not found at $CM7_IMAGE after build."
  exit 1
fi

echo "Mass erasing target"
"$STM32_PROGRAMMER_CLI" -c port="$STM32_PROGRAMMER_PORT" -e all

echo "Flashing CM4 from $CM4_IMAGE"
"$STM32_PROGRAMMER_CLI" -c port="$STM32_PROGRAMMER_PORT" -w "$CM4_IMAGE" -v

echo "Flashing CM7 from $CM7_IMAGE"
"$STM32_PROGRAMMER_CLI" -c port="$STM32_PROGRAMMER_PORT" -w "$CM7_IMAGE" -v -rst
