#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
COMPOSE_FILE="$ROOT_DIR/docker-compose.jetson.yml"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is not installed or not in PATH."
  exit 1
fi

if ! docker compose version >/dev/null 2>&1; then
  echo "docker compose plugin is not available."
  exit 1
fi

if [ ! -c /dev/net/tun ]; then
  echo "/dev/net/tun is missing. Ensure TUN/TAP is enabled in the kernel."
  exit 1
fi

if ! id -nG "$(id -un)" | tr ' ' '\n' | grep -q '^docker$'; then
  echo "User is not in the docker group. Use sudo or add the user to the docker group."
fi

if [ ! -f "$COMPOSE_FILE" ]; then
  echo "Compose file not found at $COMPOSE_FILE."
  exit 1
fi

echo "Setup checks passed."
