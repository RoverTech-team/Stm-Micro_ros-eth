#!/bin/bash
# STEP 3 — FULL arm incl. J6 (ATTENDED motion!). CM7 e2e_full + CM4 full.
# Prereq: STEP 2 passes. Requires typed confirmation (J6 will move).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BRINGUP="$(dirname "$SCRIPT_DIR")"
# shellcheck disable=SC1091
. "$SCRIPT_DIR/common.sh"

echo "########## STEP 3: FULL arm incl. J6 (ATTENDED) ##########"
echo "This will flash firmware that COMMANDS J6. Keep clear of the arm."
read -r -p "Type YES to continue: " CONFIRM
[ "$CONFIRM" = "YES" ] || { echo "aborted"; exit 1; }

echo "--- prereq: step2 net check ---"
"$SCRIPT_DIR/validate_net.sh"

echo "--- build ---"
make -C "$BRINGUP/cm7_e2e_full" -j4 > /dev/null
make -C "$BRINGUP/cm4_full" -j4 > /dev/null
echo "--- flash (mass erase + both cores) ---"
"$CLI" -c port=SWD mode=UR -e all > /dev/null
"$CLI" -c port=SWD mode=UR -w "$BRINGUP/cm7_e2e_full/build/MicroRosEth_CM7_e2e_full.elf" -v 2>&1 | tail -n 2
"$CLI" -c port=SWD mode=UR -w "$BRINGUP/cm4_full/build/MicroRosEth_CM4_full.elf" -v 2>&1 | tail -n 2
"$CLI" -c port=SWD -rst > /dev/null 2>&1
sleep 10

echo "--- validate ping after flash ---"
"$SCRIPT_DIR/validate_net.sh"

echo "--- validate all-6 motion +-10deg ---"
SERIAL_DEV="$(resolve_serial)"
export SERIAL_DEV
OUT="/tmp/step3_serial.log"
: > "$OUT"
P10="0.1745,0.1745,0.1745,0.1745,0.1745,0.1745, 0,0,0,0,0,0, 0,0,0,0,0,0"
M10="-0.1745,-0.1745,-0.1745,-0.1745,-0.1745,-0.1745, 0,0,0,0,0,0, 0,0,0,0,0,0"
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
    subprocess.run(['sshpass','-p','RoverTech','ssh','-o','StrictHostKeyChecking=no',
      os.environ.get('JETSON','RoverTech@192.168.0.1'),
      'source /opt/ros/humble/setup.bash; export ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}; /opt/ros/humble/bin/ros2 topic pub --once /joint_commands std_msgs/msg/Float32MultiArray \"{data:[' + data + ']}\"'],
      capture_output=True, timeout=30)
    time.sleep(6)
stop = True; th.join(timeout=2); s.close()
" 2>&1 | strings | grep -E "joint|protocol" | tail -n 15

RC=0
grep -a -E "protocol-ok magic=0x53485244" "$OUT" || { fail "no protocol-ok"; RC=1; }
grep -a -E "joint-cmd-shared" "$OUT" || { fail "no joint-cmd-shared"; RC=1; }
[ "$RC" -eq 0 ] && pass "STEP 3 COMPLETE (verify all 6 joints moved on hardware)" || exit 1
