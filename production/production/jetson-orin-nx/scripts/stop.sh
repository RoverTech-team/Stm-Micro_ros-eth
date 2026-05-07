#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/load_env.sh"
load_env_defaults "$ROOT_DIR/.env"
STACK="${STACK:-renode}"

COMPOSE_FILE="$("$SCRIPT_DIR/resolve-compose.sh")"
docker compose -f "$COMPOSE_FILE" down
