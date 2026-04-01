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
- `STACK=hil-macos`: macOS-hosted hardware-in-the-loop stack

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
