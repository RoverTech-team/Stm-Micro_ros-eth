---
title: REST API Reference
parent: microk3
nav_order: 2
---

# REST API Reference

Authentication uses **HTTP Basic Auth** (username/password from `.env`). Read endpoints require no auth.

---

<div class="api-section-header" style="display: flex; align-items: center; gap: 0.75rem; padding: 0.5rem 0; border-bottom: 1px solid #30363d; margin-bottom: 1rem;">
  <span class="badge" style="background: #1a3a2a; color: #3fb950; border: 1px solid #238636;">READ</span>
  <span style="font-size: 0.75rem; background: #21262d; padding: 2px 10px; border-radius: 12px; color: #8b949e;">30 req/min · no auth</span>
</div>

| Method | Endpoint | Description |
|---|---|---|
| <span class="badge badge-get">GET</span> | `/api/system_status` | System summary + ROS connection status |
| <span class="badge badge-get">GET</span> | `/api/nodes` | All registered nodes |
| <span class="badge badge-get">GET</span> | `/api/nodes/<id>` | Single node detail |
| <span class="badge badge-get">GET</span> | `/api/failures` | Full failure history |
| <span class="badge badge-get">GET</span> | `/api/nodes/<id>/logs` | Log lines filtered for node |
| <span class="badge badge-get">GET</span> | `/api/tasks` | Task status dict |
| <span class="badge badge-get">GET</span> | `/health` | Health check (rate-limit exempt) |

<br>

<div class="api-section-header" style="display: flex; align-items: center; gap: 0.75rem; padding: 0.5rem 0; border-bottom: 1px solid #30363d; margin-bottom: 1rem;">
  <span class="badge" style="background: #2d2a1a; color: #d29922; border: 1px solid #9e6a03;">WRITE</span>
  <span style="font-size: 0.75rem; background: #21262d; padding: 2px 10px; border-radius: 12px; color: #8b949e;">10 req/min · Basic Auth required</span>
</div>

| Method | Endpoint | Description |
|---|---|---|
| <span class="badge badge-post">POST</span> | `/api/update_node` | Update node status or health score |
| <span class="badge badge-post">POST</span> | `/api/add_failure` | Log a new failure for a node |

---

## Examples

```bash
# System status
curl http://localhost:5050/api/system_status

# Update node status (auth required)
curl -u admin:password \
  -X POST -H "Content-Type: application/json" \
  -d '{"node_id": 1, "status": "standby"}' \
  http://localhost:5050/api/update_node
```
