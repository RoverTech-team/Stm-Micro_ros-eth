---
title: Production Overview
parent: Production
nav_order: 1
---

# Production Deployment (Jetson Orin NX)

Production runs four Docker services on the Jetson Orin NX via `production/jetson-orin-nx/docker-compose.jetson.yml`.

## Services

| Service | Image | Role |
|---|---|---|
| `firmware-build` | `Dockerfile.firmware-build` | Compiles CM4 + CM7 ELFs on first run |
| `microk3` | `microrosWs/microk3/Dockerfile` | Flask dashboard on port 5050 |
| `renode-bridge` | Same as microk3 | Bridges Renode heartbeat → ROS 2 topics |
| `renode-e2e` | `Dockerfile.renode-e2e` | Privileged Renode sim with TAP networking |

## Startup Order

```
firmware-build (completes) 
    └── renode-e2e (starts)
    └── microk3 (health check: /health passes)
            └── renode-bridge (starts)
```

## Network

All services share the `renode_jetson_net` bridge network. TAP gateway defaults to `192.168.50.1/24`, agent port `8888`.
