# Docker-Only Renode E2E

End-to-end stack running STM32H755 Renode simulation, micro-ROS agent, and microk3 entirely in Docker. Full documentation is hosted on GitHub Pages.

## Documentation

- https://rovertech-team.github.io/Stm-Micro_ros-eth/testing/overview.html

## Quick Links

- https://rovertech-team.github.io/Stm-Micro_ros-eth/simulation/overview.html

## Firmware Selection

Use `FIRMWARE_PROJECT_DIR` to choose which firmware tree the Renode E2E stack builds and loads.

Examples:

- `docker compose -f docker-compose.renode-e2e.yml up --build`
- `FIRMWARE_PROJECT_DIR=/workspace/microrosWs/H7-Arm/H7arm docker compose -f docker-compose.renode-e2e.yml up --build`
