#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
STACK="${STACK:-renode}"

case "$STACK" in
  hil)
    COMPOSE_FILE="$ROOT_DIR/docker-compose.hil.yml"
    ;;
  renode|*)
    COMPOSE_FILE="$ROOT_DIR/docker-compose.jetson.yml"
    ;;
esac

if [ "$#" -eq 0 ]; then
  echo "Usage: $0 <service> [--tail N]"
  exit 1
fi

docker compose -f "$COMPOSE_FILE" logs "$@"
