#!/bin/bash
# STEP 1 — baseline: CM7 echo + CM4 follow.
# Validates: flash, ping, agent session, heartbeat downlink, joint echo uplink.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BRINGUP="$(dirname "$SCRIPT_DIR")"
# shellcheck disable=SC1091
. "$SCRIPT_DIR/common.sh"

echo "########## STEP 1: echo baseline ##########"
echo "--- build ---"
make -C "$BRINGUP/cm7_echo" -j4 > /dev/null
make -C "$BRINGUP/cm4_follow" -j4 > /dev/null
echo "--- flash (mass erase + both cores) ---"
"$CLI" -c port=SWD mode=UR -e all > /dev/null
"$CLI" -c port=SWD mode=UR -w "$BRINGUP/cm7_echo/build/MicroRosEth_CM7_echo.elf" -v 2>&1 | tail -n 2
"$CLI" -c port=SWD mode=UR -w "$BRINGUP/cm4_follow/build/MicroRosEth_CM4_follow.elf" -v 2>&1 | tail -n 2
"$CLI" -c port=SWD -rst > /dev/null 2>&1
sleep 8

echo "--- validate net ---"
"$SCRIPT_DIR/validate_net.sh"

echo "--- validate agent entities ---"
ssh_jetson "${ROS_SETUP}; ${ROS_ENV}; /opt/ros/humble/bin/ros2 topic list 2>&1 | grep -E 'joint_states|heartbeat'" \
  || { fail "topics missing"; exit 1; }

echo "--- validate heartbeat downlink ---"
HB="$(ssh_jetson "${ROS_SETUP}; ${ROS_ENV}; timeout 8 /opt/ros/humble/bin/ros2 topic echo /heartbeat std_msgs/msg/Int32 --qos-reliability best_effort --once 2>&1 | head -n 3" || true)"
echo "$HB"
echo "$HB" | grep -q "data:" || { fail "no heartbeat data"; exit 1; }

echo "--- validate joint echo uplink ---"
SERIAL_DEV="$(resolve_serial)"
export SERIAL_DEV
OUT="/tmp/step1_serial.log"
: > "$OUT"
timeout 30 python3 -c "
import serial, time, sys, subprocess, threading, os
s = serial.Serial(os.environ['SERIAL_DEV'], 115200, timeout=0.2)
s.reset_input_buffer()
stop = False
def reader():
    while not stop:
        d = s.read(2048)
        if d:
            sys.stdout.buffer.write(d); sys.stdout.flush()
            open('$OUT', 'ab').write(d)
th = threading.Thread(target=reader, daemon=True); th.start()
time.sleep(1)
subprocess.run(['sshpass','-p',os.environ.get('SSHPASS','RoverTech'),'ssh','-o','StrictHostKeyChecking=no',
  os.environ.get('JETSON','RoverTech@192.168.0.1'),
  'source /opt/ros/humble/setup.bash; export ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}; /opt/ros/humble/bin/ros2 topic pub --once /joint_commands std_msgs/msg/Float32MultiArray \"{data:[0.1745,0,0,0,0,0, 0,0,0,0,0,0, 0,0,0,0,0,0]}\"'],
  capture_output=True, timeout=30)
time.sleep(8)
stop = True; th.join(timeout=2); s.close()
" 2>&1 | strings | tail -n 12
grep -a -E "joint-cmd-recv" "$OUT" \
  && pass "STEP 1 COMPLETE" \
  || { fail "no joint-cmd-recv in serial (uplink broken?)"; exit 1; }
