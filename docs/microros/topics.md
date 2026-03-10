---
title: ROS 2 Topics
parent: micro-ROS
nav_order: 3
---

# ROS 2 Topics

All topics use `std_msgs/String` with a JSON payload in the `data` field.

## Topic Reference

| Topic | Direction (from dashboard) | Description |
|---|---|---|
| `microk3/node_status` | Subscribe | Node heartbeat and status updates |
| `microk3/system_alerts` | Subscribe | Failure and warning alerts |
| `microk3/commands` | Publish | Commands sent to nodes |

## Message Formats

### `microk3/node_status`
```json
{
  "id": 1,
  "status": "active",
  "health": 95,
  "uptime": "12h 34m"
}
```
If `heartbeat_raw` key is present, microk3 also logs a `RAW_HEARTBEAT` entry.

### `microk3/system_alerts`
```json
{
  "node_id": 1,
  "msg": "Overheating detected",
  "level": "warning"
}
```

### `microk3/commands`
```json
{
  "target_id": 1,
  "command": "SET_STATUS:standby"
}
```

## Node Auto-Discovery

When a new `id` appears on `microk3/node_status` that does not match any known node, microk3 **automatically creates** a new `Node` object. No manual registration is needed.
