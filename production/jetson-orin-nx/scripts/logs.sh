#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
COMPOSE_FILE="$("$SCRIPT_DIR/resolve-compose.sh")"

if [ "$#" -eq 0 ]; then
  echo "Usage: $0 <service> [--tail N]"
  exit 1
fi

docker compose -f "$COMPOSE_FILE" logs "$@"
