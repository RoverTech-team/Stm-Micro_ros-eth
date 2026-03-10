---
title: Quick Start
parent: Production
nav_order: 2
---

# Production Quick Start

## 1 — Clone and configure

```bash
git clone https://github.com/RoverTech-team/Stm-Micro_ros-eth.git
cd Stm-Micro_ros-eth/production/jetson-orin-nx

cp .env.example .env
# Edit .env — set SECRET_KEY and ADMIN_PASSWORD
```

## 2 — Start the stack

```bash
docker-compose -f docker-compose.jetson.yml up --build
```

First run compiles the firmware on-device (takes 5–10 min). Subsequent starts are fast.

## 3 — Verify

```bash
# Dashboard health
curl http://localhost:5050/health

# System status
curl http://localhost:5050/api/system_status
```

## 4 — Monitor logs

```bash
docker-compose -f docker-compose.jetson.yml logs -f microk3
docker-compose -f docker-compose.jetson.yml logs -f renode-e2e
```
