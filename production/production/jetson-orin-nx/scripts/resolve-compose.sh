#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/load_env.sh"
load_env_defaults "$ROOT_DIR/.env"
STACK="${STACK:-renode}"

case "$STACK" in
  hil)
    printf '%s\n' "$ROOT_DIR/docker-compose.hil.yml"
    ;;
  hil-host)
    printf '%s\n' "$ROOT_DIR/docker-compose.hil.yml"
    ;;
  hil-macos)
    printf '%s\n' "$ROOT_DIR/docker-compose.hil.macos.yml"
    ;;
  renode|*)
    printf '%s\n' "$ROOT_DIR/docker-compose.jetson.yml"
    ;;
esac
