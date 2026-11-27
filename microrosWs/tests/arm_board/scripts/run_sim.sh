#!/bin/bash
# Launcher for the ARM main-board sim (step1/2/3), mirroring
# microrosWs/tests/simulation/scripts/run_renode.sh.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARM_BOARD="$(dirname "$SCRIPT_DIR")"
REPO_ROOT="$(dirname "$(dirname "$(dirname "$ARM_BOARD")")")"

RENODE_DLL="${RENODE_DLL:-/Applications/renode/output/bin/Release/Renode.dll}"
STEP="step1"
TIMEOUT_VALUE="240"
HEADLESS=false
GUI=false

usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  --step step1|step2|step3   BRINGUP step (default: step1)
  --headless                run headless, grep UART for boot asserts
  --timeout SECONDS         headless virtual-log window (default: 60)
  --gui                     open Renode UI with the step script
  --renode-dll PATH         Renode.dll (default: $RENODE_DLL)
  -h, --help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --step) STEP="$2"; shift 2 ;;
        --headless) HEADLESS=true; shift ;;
        --timeout) TIMEOUT_VALUE="$2"; shift 2 ;;
        --gui) GUI=true; shift ;;
        --renode-dll) RENODE_DLL="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

case "$STEP" in
    step1) RESC="$ARM_BOARD/scripts/step1_echo.resc" ;;
    step2) RESC="$ARM_BOARD/scripts/step2_e2e.resc" ;;
    step3) RESC="$ARM_BOARD/scripts/step3_full.resc" ;;
    *) echo "Unknown step: $STEP" >&2; exit 2 ;;
esac

[[ -f "$RENODE_DLL" ]] || { echo "Renode.dll not found: $RENODE_DLL" >&2; exit 3; }
[[ -f "$RESC" ]] || { echo "resc not found: $RESC" >&2; exit 5; }

# resc paths are repo-root-relative (Renode has no $ORIGIN here)
cd "$REPO_ROOT"

if [[ "$GUI" == true ]]; then
    dotnet "$RENODE_DLL" --ui "$RESC"
    exit $?
fi

if [[ "$HEADLESS" == true ]]; then
    LOG="${LOG_FILE:-$(mktemp /tmp/arm_board_sim_XXXXXX)}"
    echo "firmware: BRINGUP $STEP | log: $LOG"
    # --console runs the script then serves the monitor; timeout bounds it.
    timeout "$TIMEOUT_VALUE" dotnet "$RENODE_DLL" --console --disable-xwt "$RESC" > "$LOG" 2>&1 || true
    echo "--- functional asserts (UART is silent in sim; see resc comments) ---"
    grep -E "RESULT" "$LOG" | head -n 20 || true
    if grep -q "RESULT shared_magic=.*OK" "$LOG" && grep -q "RESULT motor_ready=.*OK" "$LOG"; then
        echo "PASS: shared magic + motor_ready (CM4 alive, drivers configured over sim SPI)"
        exit 0
    fi
    echo "FAIL: shared/motor asserts missing in $LOG" >&2
    exit 6
fi

dotnet "$RENODE_DLL" --console "$RESC"
