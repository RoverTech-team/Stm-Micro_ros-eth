#!/usr/bin/env python3

import json
import os
import time

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile
from rclpy.qos import ReliabilityPolicy
from std_msgs.msg import Int32
from std_msgs.msg import String


class RenodeHeartbeatBridge(Node):
    def __init__(self):
        super().__init__("renode_heartbeat_bridge")

        self.node_id = int(os.environ.get("RENODE_NODE_ID", "755"))
        self.node_name = os.environ.get("RENODE_NODE_NAME", "renode-stm32h755")
        self.node_type = os.environ.get("RENODE_NODE_TYPE", "Renode STM32H755")
        self.node_network = os.environ.get("RENODE_NODE_NETWORK", "Ethernet via TAP")
        self.heartbeat_topic = os.environ.get("RENODE_HEARTBEAT_TOPIC", "heartbeat")
        self.status_topic = os.environ.get("MICROK3_STATUS_TOPIC", "microk3/node_status")
        self.alert_topic = os.environ.get("MICROK3_ALERT_TOPIC", "microk3/system_alerts")
        self.command_topic = os.environ.get("MICROK3_COMMAND_TOPIC", "microk3/commands")
        self.timeout_sec = float(os.environ.get("RENODE_HEARTBEAT_TIMEOUT_SEC", "5.0"))

        self.status_pub = self.create_publisher(String, self.status_topic, 10)
        self.alert_pub = self.create_publisher(String, self.alert_topic, 10)
        heartbeat_qos = QoSProfile(depth=10, reliability=ReliabilityPolicy.BEST_EFFORT)
        self.create_subscription(Int32, self.heartbeat_topic, self.on_heartbeat, heartbeat_qos)
        self.create_subscription(String, self.command_topic, self.on_command, 10)
        self.create_timer(1.0, self.on_timer)

        self.start_time = time.monotonic()
        self.last_heartbeat_time = None
        self.last_sequence = None
        self.offline_reported = False
        self.heartbeat_log_count = 0
        self.last_heartbeat_log_time = 0.0
        self.heartbeat_log_interval = float(os.environ.get("HEARTBEAT_LOG_INTERVAL_SEC", "30.0"))

        self.get_logger().info(
            f"Bridge online: heartbeat='{self.heartbeat_topic}' -> '{self.status_topic}' "
            f"for node_id={self.node_id}"
        )

    def on_heartbeat(self, msg: Int32):
        self.last_heartbeat_time = time.monotonic()
        self.last_sequence = int(msg.data)
        was_offline = self.offline_reported
        self.offline_reported = False
        self.heartbeat_log_count += 1
        now = time.monotonic()
        should_log = self.heartbeat_log_interval > 0 and (
            self.heartbeat_log_count <= 3 or
            (now - self.last_heartbeat_log_time) >= self.heartbeat_log_interval
        )
        if should_log:
            self.last_heartbeat_log_time = now
            self.get_logger().info(f"Heartbeat received seq={self.last_sequence}")

        self.publish_status(status="active", health=100)

        if was_offline:
            self.publish_alert(
                level="info",
                message="Renode heartbeat resumed"
            )

    def on_command(self, msg: String):
        try:
            data = json.loads(msg.data)
        except json.JSONDecodeError:
            self.get_logger().warning(f"Ignoring invalid command JSON: {msg.data}")
            return

        target_id = data.get("target_id")
        if target_id in (None, self.node_id, "all", "*"):
            self.get_logger().info(f"Observed dashboard command for simulated node: {msg.data}")

    def on_timer(self):
        if self.last_heartbeat_time is None:
            return

        elapsed = time.monotonic() - self.last_heartbeat_time
        if elapsed < self.timeout_sec:
            return

        if self.offline_reported:
            return

        self.offline_reported = True
        self.publish_status(status="offline", health=0)
        self.publish_alert(
            level="warning",
            message=f"No heartbeat received for {elapsed:.1f}s"
        )

    def publish_status(self, status: str, health: int):
        payload = {
            "id": self.node_id,
            "name": self.node_name,
            "status": status,
            "health": health,
            "uptime": self.format_uptime(),
            "type": self.node_type,
            "network": self.node_network,
        }
        if self.last_sequence is not None:
            payload["heartbeat_seq"] = self.last_sequence
            payload["heartbeat_raw"] = {
                "topic": self.heartbeat_topic,
                "type": "std_msgs/Int32",
                "data": self.last_sequence,
            }

        msg = String()
        msg.data = json.dumps(payload)
        self.status_pub.publish(msg)

    def publish_alert(self, level: str, message: str):
        payload = {
            "node_id": self.node_id,
            "level": level,
            "msg": message,
        }
        msg = String()
        msg.data = json.dumps(payload)
        self.alert_pub.publish(msg)

    def format_uptime(self) -> str:
        total = int(time.monotonic() - self.start_time)
        hours = total // 3600
        minutes = (total % 3600) // 60
        seconds = total % 60
        if hours > 0:
            return f"{hours}h {minutes}m"
        if minutes > 0:
            return f"{minutes}m {seconds}s"
        return f"{seconds}s"


def main():
    rclpy.init()
    node = RenodeHeartbeatBridge()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
