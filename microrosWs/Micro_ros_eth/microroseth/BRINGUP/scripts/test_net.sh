#!/bin/bash
# Network check from Jetson (run on Mac, SSHs to RoverTech@192.168.0.1).
# STM is on the rover LAN: 192.168.0.5 (was 192.170.0.2 on the direct link).
set -e
JETSON="${JETSON:-RoverTech@192.168.0.1}"
SSHPASS="${SSHPASS:-RoverTech}"
STM_IP="${STM_IP:-192.168.0.5}"
export SSHPASS

echo "=== Jetson LAN interface ==="
sshpass -p "$SSHPASS" ssh -o StrictHostKeyChecking=no "$JETSON" \
  "ip -br addr show | grep -E '192\\.168\\.0\\.1' | head -n 4; ip route | grep '192.168.0.0/24' | head"
echo "=== Ping STM $STM_IP (from Jetson) ==="
sshpass -p "$SSHPASS" ssh -o StrictHostKeyChecking=no "$JETSON" \
  "ping -c 3 $STM_IP 2>&1 | tail -n 6"
echo "=== ARP entry ==="
sshpass -p "$SSHPASS" ssh -o StrictHostKeyChecking=no "$JETSON" \
  "ip neigh show $STM_IP"
