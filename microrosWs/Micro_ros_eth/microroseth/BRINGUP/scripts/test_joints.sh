#!/bin/bash
# E2E joint test J1-J5 +-10deg (0.1745 rad), J6 held at 0.
# Run on Mac. Requires: e2e stack flashed, agent running on Jetson.
# Usage: ./test_joints.sh [once|+10|-10|both]
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
. "$SCRIPT_DIR/common.sh"
MODE="${1:-both}"
ROS="$ROS_SETUP; $ROS_ENV"

pub() {
  sshpass -p "$SSHPASS" ssh -o StrictHostKeyChecking=no "$JETSON" \
    "$ROS; /opt/ros/humble/bin/ros2 topic pub --once /joint_commands std_msgs/msg/Float32MultiArray \"{data:[$1]}\" 2>&1 | tail -n 2"
}

P10="0.1745,0.1745,0.1745,0.1745,0.1745,0, 0,0,0,0,0,0, 0,0,0,0,0,0"
M10="-0.1745,-0.1745,-0.1745,-0.1745,-0.1745,0, 0,0,0,0,0,0, 0,0,0,0,0,0"

case "$MODE" in
  once) pub "$P10" ;;
  +10)  pub "$P10" ;;
  -10)  pub "$M10" ;;
  both)
    echo "=== J1-J5 +10deg ==="; pub "$P10"; sleep 3
    echo "=== J1-J5 -10deg ==="; pub "$M10"
    ;;
esac
echo "=== joint_states (best_effort, once) ==="
sshpass -p "$SSHPASS" ssh -o StrictHostKeyChecking=no "$JETSON" \
  "$ROS; timeout 6 /opt/ros/humble/bin/ros2 topic echo /joint_states std_msgs/msg/Float32MultiArray --qos-reliability best_effort --once 2>&1 | head -n 8" || true
echo DONE
