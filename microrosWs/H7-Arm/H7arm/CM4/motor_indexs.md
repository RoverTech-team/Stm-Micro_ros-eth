# Motor Index Mapping (Daisy Chain)

Formula: `tx[MOT_NUMBER - 1 - active]` where `MOT_NUMBER = 6`.

| Software idx | tx byte | Physical | Notes |
|---|---|---|---|
| 0 | tx[5] | J1 | Driver present, motor absent |
| 1 | tx[4] | J4 or J5 | |
| 2 | tx[3] | J6 | |
| 3 | tx[2] | J4 or J5 | (the other) |
| 4 | tx[1] | J3 | Target for J3 movement |
| 5 | tx[0] | J2 | Target for J2 movement |

Chain from MCU outward: `J1(no motor) → J4/J5 → J6 → J4/J5 → J3 → J2`

## How to test

Sweep: `RARM_MoveDegrees(i, 10)` for i = 0..5, 2s apart.
Observe which physical motor moves for each index.
