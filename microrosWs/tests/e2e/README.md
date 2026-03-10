# Docker-Only Renode E2E

This stack runs the STM32H755 Renode simulation, the micro-ROS agent, the
`microk3` dashboard, and the Renode heartbeat bridge fully inside Docker.

## Run

From `microrosWs/tests/e2e`:

```bash
docker compose -f docker-compose.renode-e2e.yml up --build
```

If host port `5050` is already in use, override it:

```bash
DASHBOARD_PORT=5051 docker compose -f docker-compose.renode-e2e.yml up --build
```

Services:

- `firmware-build`: one-shot ARM cross-build container that produces the CM4/CM7
  ELF files into the mounted workspace
- `microk3`: Flask dashboard on `http://localhost:5050`
- `renode-e2e`: privileged Linux container that creates `tap0`, starts the
  micro-ROS agent, and launches Renode headless against the prebuilt ELFs
- `renode-bridge`: ROS 2 sidecar that converts the firmware `heartbeat` topic into
  `microk3/node_status`

## Notes

- This flow requires Docker to provide `/dev/net/tun` to the privileged
  `renode-e2e` container.
- `firmware-build` must complete successfully before `renode-e2e` starts; the
  compose file enforces this with `service_completed_successfully`.
- The firmware uses the simulation network defaults:
  - Renode STM32: `192.168.50.2`
  - Agent/TAP gateway: `192.168.50.1:8888`
- Renode logs, including mirrored `USART3` output, are available via:

```bash
docker compose -f docker-compose.renode-e2e.yml logs -f renode-e2e
```
