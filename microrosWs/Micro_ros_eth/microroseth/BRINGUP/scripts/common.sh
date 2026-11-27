#!/bin/bash
# Common helpers for BRINGUP step scripts. Source, do not execute.
# Env overrides: CLI, JETSON, SSHPASS, SERIAL_PORT

CLI="${CLI:-/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI}"
JETSON="${JETSON:-RoverTech@192.168.0.1}"
SSHPASS="${SSHPASS:-RoverTech}"
export SSHPASS
ROS_SETUP="source /opt/ros/humble/setup.bash"
# Pin DDS domain to match the authoritative agent (microk3 uros-agent, domain 0).
ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-0}"
ROS_ENV="export ROS_DOMAIN_ID=$ROS_DOMAIN_ID"
# Authoritative agent container (microk3 stack, bridge 8888:8888/udp).
AGENT_CONTAINER="uros-agent"

# Resolve serial port: env wins, else first usbmodem, else fail.
resolve_serial() {
  if [ -n "${SERIAL_PORT:-}" ] && [ -e "$SERIAL_PORT" ]; then
    echo "$SERIAL_PORT"
    return 0
  fi
  local dev
  dev="$(ls /dev/tty.usbmodem* 2>/dev/null | head -n 1)"
  if [ -n "$dev" ]; then
    echo "$dev"
    return 0
  fi
  echo "ERROR: no serial device (set SERIAL_PORT)" >&2
  return 1
}

ssh_jetson() {
  sshpass -p "$SSHPASS" ssh -o StrictHostKeyChecking=no -o ConnectTimeout=10 "$JETSON" "$@"
}

ros_pub_once() {
  # $1 = topic, $2 = type, $3 = data-json (already braced content)
  ssh_jetson "$ROS_SETUP; $ROS_ENV; /opt/ros/humble/bin/ros2 topic pub --once $1 $2 \"$3\" 2>&1 | tail -n 2"
}

# Capture serial for $1 seconds into $2, then grep $3. Prints PASS/FAIL, returns 0/1.
serial_expect() {
  local secs="$1" outfile="$2" pattern="$3"
  timeout "$secs" python3 -c "
import serial, time, sys
s = serial.Serial('$SERIAL_DEV', 115200, timeout=0.2)
s.reset_input_buffer()
t0 = time.time()
with open('$outfile', 'ab') as f:
    while time.time() - t0 < $secs:
        d = s.read(2048)
        if d:
            sys.stdout.buffer.write(d)
            sys.stdout.flush()
            f.write(d)
s.close()
" 2>&1 | strings | tail -n 30
  if grep -a -E "$pattern" "$outfile" > /dev/null 2>&1; then
    echo "PASS: serial matched /$pattern/"
    return 0
  fi
  echo "FAIL: serial did not match /$pattern/ (see $outfile)"
  return 1
}

pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; return 1; }
