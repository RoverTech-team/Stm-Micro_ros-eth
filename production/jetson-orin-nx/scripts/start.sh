#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
. "$SCRIPT_DIR/load_env.sh"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
load_env_defaults "$ROOT_DIR/.env"
STACK="${STACK:-renode}"
NO_BUILD="${NO_BUILD:-0}"
COMPOSE_FILE="$("$SCRIPT_DIR/resolve-compose.sh")"

if [ "$STACK" = "hil-macos" ]; then
  "$SCRIPT_DIR/flash_stm32_dualcore.sh"
fi

if [ "$NO_BUILD" = "1" ]; then
  docker compose -f "$COMPOSE_FILE" up -d
else
  docker compose -f "$COMPOSE_FILE" up -d --build
fi
