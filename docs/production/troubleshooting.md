---
title: Troubleshooting
parent: Production
nav_order: 5
---

# Production Troubleshooting

Common failure modes and fixes:

1. `/dev/net/tun` missing
   Fix: enable TUN/TAP and ensure containers run with the required privileges.

2. Dashboard not reachable
   Fix: confirm `DASHBOARD_PORT` and check `microk3` container logs.

3. Node not appearing in dashboard
   Fix: check Renode logs for UART output and agent connectivity.

4. Agent not listening on UDP
   Fix: verify `AGENT_PORT` and container health.

5. Renode bridge not updating status
   Fix: check `renode-bridge` logs and topic env vars.
