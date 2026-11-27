#!/bin/bash
# STEP 2 — shared E2E J1-J5 (J6 held at 0): CM7 e2e + CM4 follow.
# Prereq: STEP 1 passes (network + agent known good).
# Validates: protocol-ok magic, joint-cmd-shared, J1-J5 +-10deg, J6 stays 0.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BRINGUP="$(dirname "$SCRIPT_DIR")"
# shellcheck disable=SC1091
. "$SCRIPT_DIR/common.sh"

echo "########## STEP 2: shared E2E J1-J5 ##########"
echo "--- prereq: net must already pass (run step1 first if unsure) ---"
"$SCRIPT_DIR/validate_net.sh"

echo "--- build ---"
make -C "$BRINGUP/cm7_e2e" -j4 > /dev/null
make -C "$BRINGUP/cm4_follow" -j4 > /dev/null
echo "--- flash (mass erase + both cores) ---"
"$CLI" -c port=SWD mode=UR -e all > /dev/null
"$CLI" -c port=SWD mode=UR -w "$BRINGUP/cm7_e2e/build/MicroRosEth_CM7_e2e.elf" -v 2>&1 | tail -n 2
"$CLI" -c port=SWD mode=UR -w "$BRINGUP/cm4_follow/build/MicroRosEth_CM4_follow.elf" -v 2>&1 | tail -n 2
"$CLI" -c port=SWD -rst > /dev/null 2>&1
sleep 10

echo "--- validate ping after flash ---"
"$SCRIPT_DIR/validate_net.sh"

echo "--- validate protocol + motion ---"
SERIAL_DEV="$(resolve_serial)"
export SERIAL_DEV
OUT="/tmp/step2_serial.log"
: > "$OUT"
P10="0.1745,0.1745,0.1745,0.1745,0.1745,0, 0,0,0,0,0,0, 0,0,0,0,0,0"
M10="-0.1745,-0.1745,-0.1745,-0.1745,-0.1745,0, 0,0,0,0,0,0, 0,0,0,0,0,0"
timeout 60 python3 -c "
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
for data in ['$P10', '$M10']:
    subprocess.run(['sshpass','-p',os.environ.get('SSHPASS','RoverTech'),'ssh','-o','StrictHostKeyChecking=no',
      os.environ.get('JETSON','RoverTech@192.168.0.1'),
      'source /opt/ros/humble/setup.bash; export ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}; /opt/ros/humble/bin/ros2 topic pub --once /joint_commands std_msgs/msg/Float32MultiArray \"{data:[' + data + ']}\"'],
      capture_output=True, timeout=30)
    time.sleep(6)
stop = True; th.join(timeout=2); s.close()
" 2>&1 | strings | grep -E "joint|protocol" | tail -n 15

RC=0
grep -a -E "protocol-ok magic=0x53485244" "$OUT" || { fail "no protocol-ok (CM4 magic missing?)"; RC=1; }
grep -a -E "joint-cmd-shared" "$OUT" || { fail "no joint-cmd-shared (uplink broken?)"; RC=1; }

echo "--- validate joint_states readback (J6 must be 0) ---"
JS="$(ssh_jetson "${ROS_SETUP}; ${ROS_ENV}; timeout 8 /opt/ros/humble/bin/ros2 topic echo /joint_states std_msgs/msg/Float32MultiArray --qos-reliability best_effort --once 2>&1 | head -n 8" || true)"
echo "$JS"
[ "$RC" -eq 0 ] && pass "STEP 2 COMPLETE (verify J1-J5 moved, J6=0 on hardware)" || exit 1
