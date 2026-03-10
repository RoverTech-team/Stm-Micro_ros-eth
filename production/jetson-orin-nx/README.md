# Jetson Orin NX Production Runbook

This directory packages the full Renode + micro-ROS + microk3 E2E stack for Jetson Orin NX using on-device ARM64 builds.

## Prerequisites
- Jetson Orin NX running Ubuntu
- Docker Engine installed
- Docker Compose plugin available (`docker compose version`)
- TUN/TAP enabled (`/dev/net/tun` exists)

If Docker is not configured to run rootless, use `sudo` for the scripts or add your user to the `docker` group.

## Quick Start (Renode E2E)
1. Copy the environment template:
   - `cp .env.example .env`
2. Run setup checks:
   - `./scripts/setup.sh`
3. Start the stack:
   - `./scripts/start.sh`
   - Skip rebuilds: `NO_BUILD=1 ./scripts/start.sh`
4. Verify health:
   - `./scripts/healthcheck.sh`
5. View Renode logs:
   - `./scripts/logs.sh renode-e2e --tail 200`

Stop the stack with:
- `./scripts/stop.sh`
  - Note: this removes containers (equivalent to `docker compose down`).
  - To restart without rebuilds: `NO_BUILD=1 ./scripts/start.sh`

## Hardware-In-The-Loop (HIL)
This mode replaces Renode with a real STM32 board over Ethernet while keeping the agent, bridge, and dashboard on the Jetson.

1. Set the board to target the Jetson IP and `AGENT_PORT` (default `8888`).
   - Build the HIL firmware:
     - `make -C ../../microrosWs/Micro_ros_eth/microroseth/Makefile/CM7_hil -j$(nproc)`
     - Output: `../../microrosWs/Micro_ros_eth/microroseth/Makefile/CM7_hil/build/MicroRosEth_CM7_HIL.elf`
2. Start the HIL stack:
   - `STACK=hil ./scripts/start.sh`
   - Skip rebuilds: `STACK=hil NO_BUILD=1 ./scripts/start.sh`
3. Verify health:
   - `STACK=hil ./scripts/healthcheck.sh`
4. View logs:
   - `STACK=hil ./scripts/logs.sh micro-ros-agent --tail 200`
   - `STACK=hil ./scripts/logs.sh renode-bridge --tail 200`

Stop HIL with:
- `STACK=hil ./scripts/stop.sh`
  - Note: this removes containers (equivalent to `docker compose down`).
  - To restart without rebuilds: `STACK=hil NO_BUILD=1 ./scripts/start.sh`

## Environment Variables
Edit `.env` to override defaults:
- `DASHBOARD_PORT` (default `5050`)
- `ROS_DOMAIN_ID`
- `RENODE_NODE_ID`, `RENODE_NODE_NAME`, `RENODE_NODE_TYPE`, `RENODE_NODE_NETWORK`
- `RENODE_HEARTBEAT_TOPIC`, `RENODE_HEARTBEAT_TIMEOUT_SEC`
- `MICROK3_STATUS_TOPIC`, `MICROK3_ALERT_TOPIC`, `MICROK3_COMMAND_TOPIC`
- `AGENT_PORT`, `AGENT_BIND_ADDR`, `TAP_INTERFACE`, `TAP_GATEWAY_CIDR`
- `SECRET_KEY`, `ADMIN_USERNAME`, `ADMIN_PASSWORD`, `LOG_LEVEL`

## Troubleshooting
- If `/dev/net/tun` is missing, enable TUN/TAP in the kernel or device tree.
- If the dashboard does not load, confirm `DASHBOARD_PORT` is not used by another service.
- If the node does not appear:
  - Check `./scripts/logs.sh renode-e2e` for UART output and agent connectivity.
  - Ensure `renode-bridge` is healthy and `microk3` is running.

## Notes
- This stack builds firmware and containers on the Jetson device.
- The Renode script defaults to `/workspace/microrosWs/tests/e2e/renode/microroseth_docker_tap.resc`.
