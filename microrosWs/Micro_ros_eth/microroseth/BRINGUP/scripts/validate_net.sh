#!/bin/bash
# Validate Jetson<->STM network. Exit 0 on PASS, 1 on FAIL.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
. "$SCRIPT_DIR/common.sh"

# STM now lives on the rover LAN (192.168.0.0/24) and reaches the agent over
# that network; the old direct 192.170.0.0/24 dongle link is no longer used.
STM_IP="${STM_IP:-192.168.0.5}"
STM_MAC="${STM_MAC:-00:80:e1:00:00:00}"

echo "=== [net] Jetson LAN interface ==="
ssh_jetson "ip -br addr show | grep -E '192\\.168\\.0\\.1' | head -n 4"

echo "=== [net] ping $STM_IP ==="
PING_OUT="$(ssh_jetson "ping -c 3 -W 2 $STM_IP 2>&1 | tail -n 6" || true)"
echo "$PING_OUT"
if ! echo "$PING_OUT" | grep -qE "[1-9][0-9]* received"; then
  fail "ping got 0 replies"
  exit 1
fi
if echo "$PING_OUT" | grep -q "100% packet loss"; then
  fail "100% packet loss"
  exit 1
fi

echo "=== [net] ARP ==="
ssh_jetson "ip neigh show $STM_IP || true"
if ssh_jetson "ip neigh show $STM_IP | grep -q FAILED"; then
  fail "ARP FAILED (STM not answering ARP)"
  exit 1
fi
if ! ssh_jetson "ip neigh show $STM_IP | grep -qi '$STM_MAC'"; then
  fail "ARP MAC is not $STM_MAC (address taken by another device?)"
  exit 1
fi

echo "=== [net] single authoritative agent (microk3 uros-agent) ==="
AGENTS="$(ssh_jetson "docker ps --format '{{.Names}}' 2>/dev/null | grep -iE 'agent|uros' || true")"
echo "$AGENTS"
if echo "$AGENTS" | grep -q "micro-ros-agent"; then
  fail "production micro-ros-agent is running - stop it, final uses microk3 uros-agent (port 8888 conflict)"
  exit 1
fi
if ! echo "$AGENTS" | grep -qx "$AGENT_CONTAINER"; then
  fail "microk3 uros-agent container not running (start: microk3 checkout -> docker compose up -d uros-agent)"
  exit 1
fi

echo "=== [net] agent session ==="
ssh_jetson "docker logs uros-agent 2>&1 | grep -E 'address: $STM_IP|running....*8888' | tail -n 2" || true

pass "network OK"
