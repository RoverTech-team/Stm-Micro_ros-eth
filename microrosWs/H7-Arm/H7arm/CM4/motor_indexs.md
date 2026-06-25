# Motor Index Mapping (Daisy Chain) — CONFIRMED

Formula: `tx[MOT_NUMBER - 1 - active]` where `MOT_NUMBER = 6`.

| Sw idx | tx byte | Physical | Config | Notes |
|--------|---------|----------|--------|-------|
| 0      | tx[5]   | J1       | 3:1    | Driver present, motor absent |
| 1      | tx[4]   | J5       | 5:1    | |
| 2      | tx[3]   | J6       | 50:1   | |
| 3      | tx[2]   | J4       | 5:1    | |
| 4      | tx[1]   | J3       | 50:1   | Electromagnetic brake on PA8 |
| 5      | tx[0]   | J2       | 50:1   | Electromagnetic brake on PE11 |

Chain from MCU outward: `J2 → J3 → J4 → J6 → J5 → J1(no motor)`

## Code constants

In `main_cmdfollow.c` (and standalone variants):

```c
#define PHYS_J1 0
#define PHYS_J5 1
#define PHYS_J6 2
#define PHYS_J4 3
#define PHYS_J3 4
#define PHYS_J2 5
```
