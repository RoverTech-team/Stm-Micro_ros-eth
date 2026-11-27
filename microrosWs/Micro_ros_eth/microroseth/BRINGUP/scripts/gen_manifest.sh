#!/bin/bash
# Regenerate BUILD_MANIFEST.md with sizes + sha256 of all .elf artifacts.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BRINGUP="$(dirname "$SCRIPT_DIR")"
OUT="$BRINGUP/BUILD_MANIFEST.md"
{
  echo "# BUILD_MANIFEST"
  echo ""
  echo "Generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo ""
  echo "| variant | elf | text/data/bss | sha256 |"
  echo "|---------|-----|----------------|--------|"
  for spec in \
    "cm7_echo:MicroRosEth_CM7_echo" \
    "cm7_e2e:MicroRosEth_CM7_e2e" \
    "cm7_e2e_full:MicroRosEth_CM7_e2e_full" \
    "cm4_follow:MicroRosEth_CM4_follow" \
    "cm4_full:MicroRosEth_CM4_full"; do
    dir="${spec%%:*}"
    tgt="${spec##*:}"
    elf="$BRINGUP/$dir/build/$tgt.elf"
    if [ -f "$elf" ]; then
      size="$(arm-none-eabi-size "$elf" 2>/dev/null | tail -n 1 | awk '{print $1"/"$2"/"$3}')"
      sha="$(shasum -a 256 "$elf" | awk '{print substr($1,1,16)}')"
      echo "| $dir | $tgt.elf | $size | ${sha} |"
    else
      echo "| $dir | $tgt.elf | MISSING | - |"
    fi
  done
} > "$OUT"
cat "$OUT"
