---
title: HIL
parent: Production
nav_order: 4
---

# Hardware-In-The-Loop (HIL)

HIL replaces Renode with a real STM32 board while keeping the agent and dashboard on Jetson.

From `production/jetson-orin-nx/README.md`:

1. Build the HIL firmware:

```
make -C ../../microrosWs/Micro_ros_eth/microroseth/Makefile/CM7_hil -j$(nproc)
```

2. Start the HIL stack:

```
STACK=hil ./scripts/start.sh
```

3. Verify and inspect logs:

```
STACK=hil ./scripts/healthcheck.sh
STACK=hil ./scripts/logs.sh micro-ros-agent --tail 200
STACK=hil ./scripts/logs.sh renode-bridge --tail 200
```

TODO: Add board IP configuration steps if they are documented elsewhere.
