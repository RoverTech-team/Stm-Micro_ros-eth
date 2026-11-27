# J1 Tuning — 90° total movement (redo, all 6 powered)

**Hardware:** `NUCLEO-H755ZI-Q` CM4 `@0x08100000`, `J1 50:1 200 steps 128 µsteps`, `24V`, `rarm.h:10 J1_INDEX 0`, `rarm.h:44 int_speed`, `rarm.c:60 INT_SPEED` after `FN_SLP_DEC`, `ps01.c:154 14b`, all 6 drivers powered (`MOT_NUMBER=6`).
**WireTest:** `main_wiretest.c:21 WIRETEST_PERIOD_MS 10000` `±90°` J1 (90°=320k µsteps ~2.5s @1000, 0.6s @4000), `RARM_MoveDegrees(J1,90)` immediate at boot + periodic with `RARM_IsMoving` guard (`rarm.c:209`), brakes kept engaged (J1 no brake), `FS>max` (`FS 30000 1966 reg > max`).
**Goal:** Emanuele `fullstep_speed 4000→30000` (`>max` stays µstep) + `INT_SPEED 0x0800` (J2 `0x0D69`), total movement 90° not 15°.

## J1 90° Trial Matrix — 10 configs (flash → ask → save)

| # | J1 config `main_wiretest.c:54` | Rationale | Result |
|---|---|---|---|
| 1 | `3.5/2.5 400/400 FS10 int0 0x21/0x89` 90° | Baseline pre-Emanuele, no INT, FS<max (vibrate) |  |
| 2 | `3.5/2.5 400/400 FS4000 int0x0800 0x21/0x89` 90° | Emanuele FS4000 + INT low speed |  |
| 3 | `3.5/2.5 1000/1000 FS4000 int0x0800 0x21/0x89` 90° | Emanuele mid speed |  |
| 4 | `3.5/2.5 4000/4000 FS30000 int0x0800 0x21/0x89` 90° | Emanuele high FS30000 (capped 100000) |  |
| 5 | `3.5/2.5 1000/1000 FS30000 int0 0x21/0x89` 90° | FS30000 without INT |  |
| 6 | `4.5/3.0 1000/1000 FS30000 int0x0800` 90° | Higher KVAL torque |  |
| 7 | `2.34/1.5 400/400 FS30000 int0x0800 0x21/0x89` 90° | Mirror J5 KVAL on J1 |  |
| 8 | `3.5/2.5 1000/1000 FS20000 int0x0800` 90° | Lower FS 20000 |  |
| 9 | `3.5/2.5 1000/1000 FS30000 int0x0408` 90° | Default INT |  |
| 10| `3.5/2.5 1000/1000 FS30000 int0x0D69` 90° | J2 INT on J1 |  |

## Log
- 2026-08-30 J1 Trial 1: `3.5/2.5 400/400 FS10 int0` 90° → flashed, testing
- J1 Trial 1: `3.5/2.5 400/400 FS10 int0` 90° → no movement (user)
- J1 Trial 2: `3.5/2.5 400/400 FS4000 int0x0800` 90° → flashed
- J1 Trial 2: `3.5/2.5 400/400 FS4000 int0x0800` 90° → no movement (user)
- J1 Trial 3: `3.5/2.5 1000/1000 FS4000 int0x0800` 90° → flashed
- J1 Trial 3: `3.5/2.5 1000/1000 FS4000 int0x0800` 90° → no movement (user)
- J1 Trial 4: `3.5/2.5 4000/4000 FS30000 int0x0800` 90° → flashed
- J1 Trial 4: `3.5/2.5 4000/4000 FS30000 int0x0800` 90° → no movement (user)
- J1 Trial 5: `3.5/2.5 1000/1000 FS30000 int0` 90° → flashed
- J1 Trial5: `3.5/2.5 1000/1000 FS30000 int0` 90° → no movement (user)
- J1 Trial6: `4.5/3.0 1000/1000 FS30000 int0x0800` 90° → flashed
- J1 Trial6: `4.5/3.0 1000/1000 FS30000 int0x0800` 90° → no movement (user)
- J1 Trial7: `2.34/1.5 1000/1000 FS30000 int0x0800` 90° mirror J5 → flashed
- J1 Trial7: `2.34/1.5 1000/1000 FS30000 int0x0800` 90° mirror J5 → no movement (user)
- J1 Trial8: `3.5/2.5 1000/1000 FS20000 int0x0800` 90° → flashed
- J1 Trial8: `3.5/2.5 1000/1000 FS20000 int0x0800` 90° → no movement (user)
- J1 Trial9: `3.5/2.5 1000/1000 FS30000 int0x0408` 90° → flashed
- J1 Trial9: `3.5/2.5 1000/1000 FS30000 int0x0408` 90° → no movement (user)
- J1 Trial10: `3.5/2.5 1000/1000 FS30000 int0x0D69` 90° → flashed
- J1 Trial10: `3.5/2.5 1000/1000 FS30000 int0x0D69` 90° → no movement (user) — all 10 J1 90° trials stalled, high-ratio 50:1 not moving even with FS>max, likely hardware (brake/power/phase) not BEMF
- Conclusion: J5 5:1 FS30000 int0800 moved smooth (validated Emanuele), J1 50:1 needs hardware check (swap motor/driver, verify 24V, check brake line, check SPI chain with all 6 powered)
