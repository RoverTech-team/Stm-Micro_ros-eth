# cm7_echo — BRINGUP step 1 (CM7 net + echo, no motion)

Variant of the arm bringup tree. See `BRINGUP/README.md` + `STEPS.md`.
Behavior: heartbeat + `joint_commands` → `joint_states` ECHO, no shared-memory writes.
Build: `make -C cm7_echo -j4`. Flash `@0x08000000` via `scripts/flash_echo.sh`.
