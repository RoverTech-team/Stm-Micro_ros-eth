#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
STACK="${STACK:-renode}"
NO_BUILD="${NO_BUILD:-0}"

case "$STACK" in
  hil)
    COMPOSE_FILE="$ROOT_DIR/docker-compose.hil.yml"
    ;;
  renode|*)
    COMPOSE_FILE="$ROOT_DIR/docker-compose.jetson.yml"
    ;;
esac

if [ "$NO_BUILD" = "1" ]; then
  docker compose -f "$COMPOSE_FILE" up -d
else
  docker compose -f "$COMPOSE_FILE" up -d --build
fi
