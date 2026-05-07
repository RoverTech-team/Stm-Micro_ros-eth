# Jetson Orin NX Production Runbook

Deployment bundle for Renode + micro-ROS agent + microk3 dashboard on Jetson Orin NX. Full documentation is hosted on GitHub Pages.

## Documentation

- https://rovertech-team.github.io/Stm-Micro_ros-eth/production/overview.html

## Quick Links

- https://rovertech-team.github.io/Stm-Micro_ros-eth/production/quickstart.html
- https://rovertech-team.github.io/Stm-Micro_ros-eth/production/configuration.html
- https://rovertech-team.github.io/Stm-Micro_ros-eth/production/troubleshooting.html

## Supported Stacks

- `STACK=renode`: Jetson-hosted Renode production stack
- `STACK=hil`: Jetson-hosted hardware-in-the-loop stack
- `STACK=hil-host`: Jetson-hosted Docker HIL stack that joins the existing host ROS 2 graph
- `STACK=hil-macos`: macOS-hosted hardware-in-the-loop stack

## Jetson host-ROS HIL

The `hil-host` stack keeps `micro-ros-agent`, `microk3`, and `renode-bridge` inside Docker, but runs them on `network_mode: host` so they attach to the Jetson's existing ROS 2 graph.
It is intended for the production case where the Jetson already runs ROS 2 for other roles and the HIL services need to interoperate with that host instance directly.

When using `STACK=hil-host`:

- `./scripts/setup.sh` validates Docker and blocks `ROS_LOCALHOST_ONLY=1`, because that would isolate the containers from the host ROS graph
- `./scripts/start.sh` starts [docker-compose.hil-host.yml](/Users/giuliomastromartino/Documents/Polispace/Stm-Micro_ros-eth/production/jetson-orin-nx/docker-compose.hil-host.yml)
- `./scripts/stop.sh`, `./scripts/status.sh`, and `./scripts/logs.sh` use normal `docker compose` control flow
- `micro-ros-agent` still runs in a minimal ROS 2 container, not on the host
- `microk3` and `renode-bridge` also stay in containers, but inherit the host ROS settings through `.env`
- `hil-host` is the production integration mode when the Jetson already has an active ROS 2 environment
- `hil` remains the legacy standalone containerized HIL stack

For `hil-host`, make the container ROS settings match the host ROS 2 instance:

- `ROS_DOMAIN_ID`
- `ROS_LOCALHOST_ONLY=0`
- `RMW_IMPLEMENTATION`
- `CYCLONEDDS_URI` or `FASTRTPS_DEFAULT_PROFILES_FILE` if your host ROS 2 stack depends on them
- any discovery-specific variables such as `ROS_DISCOVERY_SERVER`, `ROS_AUTOMATIC_DISCOVERY_RANGE`, or `ROS_STATIC_PEERS`

The containers do not need the host workspace sourced. They only need compatible DDS/runtime environment so they can join the same ROS 2 graph over host networking.

## macOS HIL

The `hil-macos` stack runs the production HIL containers on an Apple Silicon macOS machine using Docker Desktop.
It is separate from the Jetson `hil` stack and does not replace it.

When using `STACK=hil-macos`:

- `./scripts/start.sh` flashes both STM32H755 cores before starting Docker services
- flashing uses STM32CubeProgrammer CLI on macOS only
- the scripts first try `STM32_Programmer_CLI` from `PATH`, then auto-detect it on `/Volumes/Untitled`
- `MAC_HOST_IP` sets the micro-ROS agent IP compiled into the CM7 HIL firmware
- `STM_BOARD_IP` sets the STM32 board IP compiled into the CM7 HIL firmware
- `CM4_MAKE_DIR`, `CM4_IMAGE`, `CM7_MAKE_DIR`, and `CM7_IMAGE` select which firmware tree is built and flashed
- `hil-macos` now rebuilds CM4 and CM7 before flashing so the shared-memory producer and consumer stay aligned
- use those `.env` values to point the scripts at `H7-Arm` or `Micro_ros_eth`
- `SERIAL_MONITOR=1` opens a macOS Terminal window with a serial monitor after startup
- the scripts auto-detect `/dev/cu.usbmodem*` by default; override with `SERIAL_PORT`
- `SERIAL_BAUD` defaults to `115200`
- the dashboard is published on `localhost:${DASHBOARD_PORT:-5050}`
- the micro-ROS agent UDP port is published on `${AGENT_PORT:-8888}`
- no firmware build step is included; the STM board is assumed to be already flashed and functional
- the board must be configured to connect to the macOS machine LAN IP address
