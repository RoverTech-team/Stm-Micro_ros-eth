#!/bin/sh
set -eu

# Install the microk3 systemd autolaunch unit (system scope).
# Source is git-tracked at production/jetson-orin-nx/systemd/microk3.service
# Jetson path: ~/Stm-Micro_ros-eth/production/jetson-orin-nx

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC_UNIT="$ROOT_DIR/systemd/microk3.service"
DST_UNIT="/etc/systemd/system/microk3.service"

if [ ! -f "$SRC_UNIT" ]; then
  echo "Source unit not found: $SRC_UNIT"
  exit 1
fi

# Require docker to be installed (matches scripts/setup.sh checks)
if ! command -v docker >/dev/null 2>&1; then
  echo "docker not found in PATH"
  exit 1
fi

# Need sudo for /etc/systemd/system
if [ "$(id -u)" -ne 0 ]; then
  SUDO="sudo"
else
  SUDO=""
fi

echo "==> Installing $SRC_UNIT -> $DST_UNIT"
$SUDO cp "$SRC_UNIT" "$DST_UNIT"
$SUDO chmod 644 "$DST_UNIT"

echo "==> Reloading systemd"
$SUDO systemctl daemon-reload

echo "==> Enabling microk3.service (auto-start at boot)"
$SUDO systemctl enable microk3.service

echo "==> Starting microk3.service now"
if ! $SUDO systemctl start microk3.service; then
  echo "Start failed, showing status:"
  $SUDO systemctl status microk3.service --no-pager || true
  $SUDO journalctl -u microk3 --no-pager -n 50 || true
  exit 1
fi

echo "==> Status"
$SUDO systemctl status microk3.service --no-pager || true
echo ""
echo "Done. Verify with:"
echo "  systemctl is-enabled microk3"
echo "  systemctl status microk3"
echo "  journalctl -u microk3 -f"
echo "  curl -sk https://\$(hostname -I | awk '{print \$1}'):5050/health"
echo "  curl http://\$(hostname -I | awk '{print \$1}'):8080/health"
echo "  docker ps --format 'table {{.Names}}\t{{.Status}}'"
