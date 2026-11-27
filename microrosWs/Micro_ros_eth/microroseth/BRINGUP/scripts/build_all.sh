#!/bin/bash
# Build all 5 firmware variants + regenerate manifest.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BRINGUP="$(dirname "$SCRIPT_DIR")"
for d in cm7_echo cm7_e2e cm7_e2e_full cm4_follow cm4_full; do
  echo "=== $d ==="
  make -C "$BRINGUP/$d" -j4 2>&1 | tail -n 2
done
"$SCRIPT_DIR/gen_manifest.sh"
