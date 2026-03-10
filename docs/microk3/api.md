---
title: REST API Reference
parent: microk3
nav_order: 2
---

# REST API Reference

Authentication uses **HTTP Basic Auth** (username/password from `.env`). Read endpoints require no auth.

## Read Endpoints (no auth, 30 req/min)

| Method | Endpoint | Description |
|---|---|---|
| GET | `/api/system_status` | System summary + ROS connection status |
| GET | `/api/nodes` | All registered nodes |
| GET | `/api/nodes/<id>` | Single node detail |
| GET | `/api/failures` | Full failure history |
| GET | `/api/nodes/<id>/logs` | Log lines filtered for node |
| GET | `/api/tasks` | Task status dict |
| GET | `/health` | Health check (rate-limit exempt) |

## Write Endpoints (Basic Auth required, 10 req/min)

| Method | Endpoint | Description |
|---|---|---|
| POST | `/api/update_node` | Update node status or health score |
| POST | `/api/add_failure` | Log a new failure for a node |

## Examples

```bash
# System status
curl http://localhost:5050/api/system_status

# Update node status (auth required)
curl -u admin:password \
  -X POST -H "Content-Type: application/json" \
  -d '{"node_id": 1, "status": "standby"}' \
  http://localhost:5050/api/update_node

# Log a failure (auth required)
curl -u admin:password \
  -X POST -H "Content-Type: application/json" \
  -d '{"node_id": 1, "description": "Temperature threshold exceeded", "status": "open"}' \
  http://localhost:5050/api/add_failure
```

## `/api/system_status` Response

```json
{
  "status": "active",
  "nodes_online": 2,
  "total_nodes": 3,
  "tasks_running": 1,
  "network_latency": 0,
  "timestamp": "2026-03-10T20:00:00",
  "ros_connected": true
}
```

## Environment Variables (`.env`)

| Variable | Default | Description |
|---|---|---|
| `SECRET_KEY` | Required | Flask session secret |
| `ADMIN_USERNAME` | `admin` | Basic Auth username |
| `ADMIN_PASSWORD` | Required | Basic Auth password |
| `LOG_LEVEL` | `INFO` | Logging verbosity |
| `ROS_DOMAIN_ID` | `0` | ROS 2 domain ID |
| `FLASK_HOST` | `127.0.0.1` | Bind address |
| `FLASK_PORT` | `5050` | Server port |
| `DATA_FILE` | `data/system_data.json` | State persistence path |
| `LOG_FILE` | `logs/microk3.log` | Log output path |
