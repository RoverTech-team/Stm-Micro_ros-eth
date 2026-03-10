---
title: Production Overview
parent: Production
nav_order: 1
---

# Production Overview
{: .no_toc }

Deployment notes for the Jetson Orin NX stack and Hardware-in-the-Loop (HIL) workflows.

---

## Startup Sequence

The system follows a strict dependency chain during startup to ensure all network bridges are ready before the firmware simulation or hardware begins publishing.

```mermaid
flowchart TD
    FB["firmware-build\n(completes)"]
    RE["renode-e2e\n(starts)"]
    MK["microk3\n✓ /health passes"]
    RB["renode-bridge\n(starts)"]

    FB --> RE
    FB --> MK
    MK --> RB

    style FB fill:#1a3a2a,stroke:#2ea44f,color:#cdd9e5
    style RE fill:#1f3a5f,stroke:#388bfd,color:#cdd9e5
    style MK fill:#1f3a5f,stroke:#388bfd,color:#cdd9e5
    style RB fill:#2d2a1a,stroke:#d29922,color:#cdd9e5
```

---

## Services Reference

| Service | Image | Port | Role |
|---|---|---|---|
| `firmware-build` | `Dockerfile.firmware-build` | — | Compiles CM4 + CM7 ELFs on first run |
| `microk3` | `microrosWs/microk3/Dockerfile` | **5050** | Flask dashboard & ROS bridge |
| `renode-bridge` | Same as microk3 | — | Bridges Renode heartbeat → ROS 2 topics |
| `renode-e2e` | `Dockerfile.renode-e2e` | — | Privileged Renode sim with TAP networking |

---

## Deployment Modes

1.  **Simulation Mode**: Everything runs in Docker on the Jetson Orin NX using Renode.
2.  **HIL Mode**: `renode-e2e` is disabled; STM32H7 hardware is connected via physical Ethernet.
