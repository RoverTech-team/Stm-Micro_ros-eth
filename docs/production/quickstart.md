---
title: Quick Start
parent: Production
nav_order: 2
---

# Quick Start (Renode E2E)

From `production/jetson-orin-nx/README.md`:

1. `cp .env.example .env`
2. `./scripts/setup.sh`
3. `./scripts/start.sh`
4. `./scripts/healthcheck.sh`
5. `./scripts/logs.sh renode-e2e --tail 200`

Stop the stack with `./scripts/stop.sh`.
