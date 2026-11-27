# cm7_e2e — BRINGUP step 2 (CM7 shared J1–J5, J6 held at 0)

Variant of the arm bringup tree. See `BRINGUP/README.md` + `STEPS.md`.
Behavior: magic-gated (`0x53485244`/v2) shared write J1–J5 to SRAM4 `0x38000000`,
J6 forced 0, on-demand `/joint_states` publish from shared.
Build: `make -C cm7_e2e -j4`. Flash `@0x08000000` via `scripts/flash_e2e.sh`.
