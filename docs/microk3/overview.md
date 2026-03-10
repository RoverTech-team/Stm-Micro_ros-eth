---
title: microk3 Overview
parent: microk3
nav_order: 1
---

# microk3 Overview
{: .no_toc }

**microk3** is a lightweight Flask-based dashboard and rclpy-powered bridge that monitors micro-ROS nodes.

---

## Live Dashboard
{: .fs-6 }

The dashboard provides a real-time view of all nodes, their health scores, and recent failures.

![microk3 Dashboard Screenshot]({{ '/assets/images/Screenshot 2026-03-10 at 22.21.08.png' | relative_url }})
*Above: The microk3 web interface showing active nodes and system status.*

---

## Architecture

```mermaid
graph LR
    STM32[STM32H7] -- UDP --> Agent[micro-ROS Agent]
    Agent -- ROS 2 --> Bridge[microk3 Bridge]
    Bridge -- JSON --> UI[microk3 Flask UI]
    Bridge -- SQLite --> DB[(Database)]
```

The bridge task (powered by `rclpy`) subscribes to ROS 2 topics and persists status updates to a local JSON file or SQLite database.

---

## REST API Integration

The dashboard exposes a REST API that can be consumed by other services for automated health monitoring.

[View API Reference](api.html){: .btn .btn-outline }

---

## Quick Setup

```bash
cd microk3
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python3 app.py
```
