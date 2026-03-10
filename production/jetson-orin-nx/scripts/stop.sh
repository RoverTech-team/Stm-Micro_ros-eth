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

docker compose -f "$COMPOSE_FILE" down
