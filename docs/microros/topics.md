---
title: Topics and Env Vars
parent: micro-ROS
nav_order: 3
---

# Topics and Environment Variables

## ROS 2 Topics

Default topics used by the Renode bridge and dashboard:

- `heartbeat`
- `microk3/node_status`
- `microk3/system_alerts`
- `microk3/commands`

These are configurable via environment variables.

## Environment Variables

From `microrosWs/microk3/docker-compose.yml`:

- `ROS_DOMAIN_ID`
- `RENODE_NODE_ID`
- `RENODE_NODE_NAME`
- `RENODE_NODE_TYPE`
- `RENODE_NODE_NETWORK`
- `RENODE_HEARTBEAT_TOPIC`
- `RENODE_HEARTBEAT_TIMEOUT_SEC`
- `MICROK3_STATUS_TOPIC`
- `MICROK3_ALERT_TOPIC`
- `MICROK3_COMMAND_TOPIC`

TODO: Document any firmware-side topic names or XRCE-DDS session identifiers.
