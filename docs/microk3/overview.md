---
title: microk3 Overview
parent: microk3
nav_order: 1
---

# microk3 Dashboard

microk3 is a **Flask + rclpy** web application that monitors ROS 2 nodes in real time. It subscribes to micro-ROS topics and exposes a REST API plus a Jinja2 HTML dashboard.

## Architecture

```
rclpy (MicroK3RosNode)
  ├── Subscribes: microk3/node_status
  ├── Subscribes: microk3/system_alerts
  └── Publishes:  microk3/commands
        │
        └── ros_update_callback() → Flask system_data (in-memory + JSON file)
                                        └── REST API / HTML templates
```

## Key Behaviours

- **Auto-discovery**: New node IDs appearing on `microk3/node_status` are automatically registered — no pre-configuration needed.
- **No cached state**: On startup microk3 always starts with an empty node list and waits for live ROS discovery. Persisted JSON is ignored for nodes.
- **Duplicate failure suppression**: Consecutive identical failures are deduplicated automatically.
- **Rate limiting**: Read endpoints 30/min, write endpoints 10/min.

## Running

```bash
# Docker (recommended)
cd microrosWs/microk3
docker-compose up --build

# Native (requires ROS 2 Humble sourced)
source /opt/ros/humble/setup.bash
pip install -r requirements.txt
python app.py
```

Access at: `http://localhost:5050`
