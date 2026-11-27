#!/bin/sh
set -eu

# Uninstall the microk3 systemd autolaunch unit.

DST_UNIT="/etc/systemd/system/microk3.service"

if [ "$(id -u)" -ne 0 ]; then
  SUDO="sudo"
else
  SUDO=""
fi

echo "==> Stopping and disabling microk3.service (if present)"
$SUDO systemctl disable --now microk3.service 2>/dev/null || true

if [ -f "$DST_UNIT" ]; then
  echo "==> Removing $DST_UNIT"
  $SUDO rm -f "$DST_UNIT"
  $SUDO systemctl daemon-reload
  echo "Removed."
else
  echo "No unit at $DST_UNIT"
fi

echo "Stack still runnable via: ./scripts/start.sh (STACK=hil-host) / ./scripts/stop.sh"
