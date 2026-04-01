#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
COMPOSE_FILE="$("$SCRIPT_DIR/resolve-compose.sh")"

docker compose -f "$COMPOSE_FILE" ps
