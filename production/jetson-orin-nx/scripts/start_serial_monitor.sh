#!/bin/sh
set -eu

SERIAL_PORT="${SERIAL_PORT:-}"
SERIAL_BAUD="${SERIAL_BAUD:-115200}"
SERIAL_SCREEN_BIN="${SERIAL_SCREEN_BIN:-$(command -v screen || true)}"

find_serial_port() {
  for candidate in /dev/cu.usbmodem* /dev/cu.usbserial*; do
    if [ -e "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  return 1
}

if [ -z "$SERIAL_PORT" ]; then
  SERIAL_PORT="$(find_serial_port || true)"
fi

if [ -z "$SERIAL_PORT" ]; then
  echo "No serial device found. Set SERIAL_PORT to override."
  exit 1
fi

if [ ! -e "$SERIAL_PORT" ]; then
  echo "Serial device $SERIAL_PORT was not found."
  exit 1
fi

if [ -z "$SERIAL_SCREEN_BIN" ]; then
  echo "screen is required to launch the serial monitor."
  exit 1
fi

SESSION_NAME="stm32-serial-monitor"
SCREEN_CMD=$(printf "'%s' -S %s -L -Logfile '/tmp/%s.log' '%s' '%s'" \
  "$SERIAL_SCREEN_BIN" "$SESSION_NAME" "$SESSION_NAME" "$SERIAL_PORT" "$SERIAL_BAUD")
ESCAPED_SCREEN_CMD=$(printf '%s' "$SCREEN_CMD" | sed 's/\\/\\\\/g; s/"/\\"/g')

osascript \
  -e 'tell application "Terminal"' \
  -e 'activate' \
  -e "do script \"$ESCAPED_SCREEN_CMD\"" \
  -e 'end tell'

echo "Serial monitor launched in Terminal on $SERIAL_PORT @ $SERIAL_BAUD."
echo "Command: $SCREEN_CMD"
