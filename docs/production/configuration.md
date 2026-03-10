---
title: Configuration
parent: Production
nav_order: 3
---

# Production Configuration

All values are set via environment variables in `.env` or the compose file.

## Environment Variables

| Variable | Default | Description |
|---|---|---|
| `SECRET_KEY` | — (required) | Flask session secret key |
| `ADMIN_USERNAME` | `admin` | Dashboard admin login |
| `ADMIN_PASSWORD` | `testpass123` | Dashboard admin password |
| `DASHBOARD_PORT` | `5050` | Host port for dashboard |
| `LOG_LEVEL` | `INFO` | Logging verbosity |
| `ROS_DOMAIN_ID` | `0` | ROS 2 domain isolation |
| `TAP_INTERFACE` | `tap0` | TAP interface name |
| `TAP_GATEWAY_CIDR` | `192.168.50.1/24` | TAP network gateway |
| `AGENT_PORT` | `8888` | micro-ROS agent UDP port |
| `RENODE_NODE_ID` | `755` | Node ID reported by Renode sim |
| `RENODE_NODE_NAME` | `renode-stm32h755` | Node display name |
| `RENODE_HEARTBEAT_TIMEOUT_SEC` | `5.0` | Seconds before node marked offline |
| `RENODE_SCRIPT` | `microroseth_docker_tap.resc` | Renode script to run |
| `CM7_ELF` | `...MicroRosEth_CM7.elf` | Path to CM7 firmware ELF |
| `CM4_ELF` | `...MicroRosEth_CM4.elf` | Path to CM4 firmware ELF |

## Chaos / Resilience Testing Variables

| Variable | Default | Effect |
|---|---|---|
| `AGENT_RESTART_AFTER_SEC` | `0` (off) | Restart agent after N seconds |
| `AGENT_RESTART_DOWNTIME_SEC` | `0` | Agent downtime duration |
| `TAP_FLAP_AFTER_SEC` | `0` (off) | Bring TAP down after N seconds |
| `RENODE_RESTART_AFTER_SEC` | `0` (off) | Restart Renode after N seconds |
