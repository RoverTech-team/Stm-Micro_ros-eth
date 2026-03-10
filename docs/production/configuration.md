---
title: Configuration
parent: Production
nav_order: 3
---

# Configuration (.env)

Defaults from `production/jetson-orin-nx/.env.example`:

- `DASHBOARD_PORT=5050`
- `ROS_DOMAIN_ID=0`
- `RENODE_NODE_ID=755`
- `RENODE_NODE_NAME=renode-stm32h755`
- `RENODE_NODE_TYPE=Renode STM32H755`
- `RENODE_NODE_NETWORK=Docker Renode TAP`
- `RENODE_HEARTBEAT_TOPIC=heartbeat`
- `RENODE_HEARTBEAT_TIMEOUT_SEC=5.0`
- `MICROK3_STATUS_TOPIC=microk3/node_status`
- `MICROK3_ALERT_TOPIC=microk3/system_alerts`
- `MICROK3_COMMAND_TOPIC=microk3/commands`
- `HEARTBEAT_LOG_INTERVAL_SEC=30`
- `AGENT_PORT=8888`
- `AGENT_BIND_ADDR=0.0.0.0`
- `TAP_INTERFACE=tap0`
- `TAP_GATEWAY_CIDR=192.168.50.1/24`
- `SECRET_KEY=e2e-test-secret-key`
- `ADMIN_USERNAME=admin`
- `ADMIN_PASSWORD=testpass123`
- `LOG_LEVEL=INFO`
