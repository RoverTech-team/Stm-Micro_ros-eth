#!/usr/bin/env python3

import json
import os
import time
from collections import deque
from datetime import datetime
from statistics import pstdev
from typing import Deque, Dict, Optional

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile
from rclpy.qos import ReliabilityPolicy
from std_msgs.msg import Int32
from std_msgs.msg import String


def utc_timestamp() -> str:
    return datetime.utcnow().isoformat() + "Z"


class TopicTelemetryState:
    WINDOW_SIZE = 30

    def __init__(self, logical_topic: str):
        self.logical_topic = logical_topic
        self.last_receive_monotonic: Optional[float] = None
        self.last_sequence: Optional[int] = None
        self.last_publish_us: Optional[int] = None
        self.last_value = None
        self.sequence_gaps = 0
        self.lag_ms = 0.0
        self.raw_delta_ms = 0.0
        self.jitter_ms = 0.0
        self.rate_hz = 0.0
        self.bandwidth_bps = 0.0
        self._samples: Deque[Dict[str, float]] = deque(maxlen=self.WINDOW_SIZE)
        self._intervals_ms: Deque[float] = deque(maxlen=self.WINDOW_SIZE)

    def update(
        self,
        sample: Dict[str, object],
        now_monotonic: float,
        payload_bytes: int,
        clock_offset_us: Optional[float],
    ) -> None:
        sequence = int(sample.get("seq", 0))
        publish_us = int(sample.get("publish_us", 0))
        value = sample.get("value")
        receive_us = now_monotonic * 1000000.0
        self.raw_delta_ms = (receive_us - float(publish_us)) / 1000.0

        if self.last_sequence is not None and sequence > self.last_sequence + 1:
            self.sequence_gaps += sequence - self.last_sequence - 1
        if clock_offset_us is None:
            self.lag_ms = 0.0
        else:
            self.lag_ms = max(0.0, receive_us - (float(publish_us) + clock_offset_us)) / 1000.0

        if self.last_receive_monotonic is not None:
            interval_ms = (now_monotonic - self.last_receive_monotonic) * 1000.0
            self._intervals_ms.append(interval_ms)
            if self._intervals_ms:
                self.jitter_ms = pstdev(self._intervals_ms) if len(self._intervals_ms) > 1 else 0.0

        self._samples.append({
            "receive_monotonic": now_monotonic,
            "bytes": float(payload_bytes),
        })
        if len(self._samples) > 1:
            elapsed = self._samples[-1]["receive_monotonic"] - self._samples[0]["receive_monotonic"]
            if elapsed > 0.0:
                self.rate_hz = (len(self._samples) - 1) / elapsed
                self.bandwidth_bps = sum(entry["bytes"] for entry in self._samples) / elapsed

        self.last_receive_monotonic = now_monotonic
        self.last_sequence = sequence
        self.last_publish_us = publish_us
        self.last_value = value

    def as_dict(self, now_monotonic: float) -> Dict[str, object]:
        stale_sec = None
        if self.last_receive_monotonic is not None:
            stale_sec = now_monotonic - self.last_receive_monotonic
        return {
            "topic": self.logical_topic,
            "last_seq": self.last_sequence,
            "last_value": self.last_value,
            "last_publish_us": self.last_publish_us,
            "lag_ms": round(self.lag_ms, 3),
            "raw_delta_ms": round(self.raw_delta_ms, 3),
            "jitter_ms": round(self.jitter_ms, 3),
            "rate_hz": round(self.rate_hz, 3),
            "bandwidth_bps": round(self.bandwidth_bps, 3),
            "sequence_gaps": self.sequence_gaps,
            "stale": stale_sec is None or stale_sec > 2.0,
            "last_rx_age_sec": round(stale_sec, 3) if stale_sec is not None else None,
        }


class RenodeHeartbeatBridge(Node):
    def __init__(self):
        super().__init__("renode_heartbeat_bridge")

        self.node_id = int(os.environ.get("RENODE_NODE_ID", "755"))
        self.node_name = os.environ.get("RENODE_NODE_NAME", "stm32h755")
        self.node_type = os.environ.get("RENODE_NODE_TYPE", "STM32H755")
        self.node_network = os.environ.get("RENODE_NODE_NETWORK", "Ethernet")
        self.heartbeat_topic = os.environ.get("RENODE_HEARTBEAT_TOPIC", "heartbeat")
        self.heartbeat_telemetry_topic = os.environ.get(
            "RENODE_HEARTBEAT_TELEMETRY_TOPIC",
            "heartbeat_telemetry",
        )
        self.position_telemetry_topic = os.environ.get(
            "RENODE_POSITION_TELEMETRY_TOPIC",
            "measured_position_telemetry",
        )
        self.time_sync_request_topic = os.environ.get(
            "MICROK3_TIME_SYNC_REQUEST_TOPIC",
            "time_sync_request",
        )
        self.time_sync_echo_topic = os.environ.get(
            "MICROK3_TIME_SYNC_ECHO_TOPIC",
            "time_sync_echo",
        )
        self.status_topic = os.environ.get("MICROK3_STATUS_TOPIC", "microk3/node_status")
        self.alert_topic = os.environ.get("MICROK3_ALERT_TOPIC", "microk3/system_alerts")
        self.command_topic = os.environ.get("MICROK3_COMMAND_TOPIC", "microk3/commands")
        self.metrics_topic = os.environ.get("MICROK3_METRICS_TOPIC", "microk3/performance_metrics")
        self.timeout_sec = float(os.environ.get("RENODE_HEARTBEAT_TIMEOUT_SEC", "5.0"))
        self.publish_interval_sec = float(os.environ.get("MICROK3_METRICS_PUBLISH_SEC", "1.0"))

        self.status_pub = self.create_publisher(String, self.status_topic, 10)
        self.alert_pub = self.create_publisher(String, self.alert_topic, 10)
        self.metrics_pub = self.create_publisher(String, self.metrics_topic, 10)
        self.time_sync_request_pub = self.create_publisher(String, self.time_sync_request_topic, 10)
        heartbeat_qos = QoSProfile(depth=10, reliability=ReliabilityPolicy.BEST_EFFORT)
        self.create_subscription(Int32, self.heartbeat_topic, self.on_heartbeat, heartbeat_qos)
        self.create_subscription(String, self.heartbeat_telemetry_topic, self.on_heartbeat_telemetry, 10)
        self.create_subscription(String, self.position_telemetry_topic, self.on_position_telemetry, 10)
        self.create_subscription(String, self.time_sync_echo_topic, self.on_time_sync_echo, 10)
        self.create_subscription(String, self.command_topic, self.on_command, 10)
        self.create_timer(1.0, self.on_timer)
        self.create_timer(self.publish_interval_sec, self.publish_metrics)
        self.create_timer(1.0, self.publish_time_sync_request)

        self.start_time = time.monotonic()
        self.last_heartbeat_time = None
        self.last_sequence = None
        self.offline_reported = False
        self.heartbeat_log_count = 0
        self.last_heartbeat_log_time = 0.0
        self.heartbeat_log_interval = float(os.environ.get("HEARTBEAT_LOG_INTERVAL_SEC", "30.0"))
        self.time_sync_sequence = 0
        self.time_sync_sent: Dict[int, float] = {}
        self.clock_offset_us: Optional[float] = None
        self.time_sync_rtt_ms: Optional[float] = None
        self.time_sync_sample_count = 0
        self.topic_states = {
            "heartbeat": TopicTelemetryState("heartbeat"),
            "measured_position": TopicTelemetryState("measured_position"),
        }

        self.get_logger().info(
            f"Bridge online: heartbeat='{self.heartbeat_topic}', telemetry='{self.heartbeat_telemetry_topic}', "
            f"position='{self.position_telemetry_topic}', sync='{self.time_sync_request_topic}'/'{self.time_sync_echo_topic}' "
            f"-> '{self.metrics_topic}'"
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
            self.publish_alert(level="info", message="Heartbeat resumed")

    def on_heartbeat_telemetry(self, msg: String):
        self._process_telemetry("heartbeat", msg)

    def on_position_telemetry(self, msg: String):
        self._process_telemetry("measured_position", msg)

    def _process_telemetry(self, logical_topic: str, msg: String):
        try:
            payload = json.loads(msg.data)
        except json.JSONDecodeError:
            self.get_logger().warning(
                f"Ignoring invalid telemetry JSON on {logical_topic}: {msg.data}"
            )
            return

        state = self.topic_states[logical_topic]
        state.update(payload, time.monotonic(), len(msg.data.encode("utf-8")), self.clock_offset_us)

    def publish_time_sync_request(self):
        host_send_us = time.monotonic_ns() // 1000
        payload = {
            "seq": self.time_sync_sequence,
            "host_send_us": host_send_us,
        }
        msg = String()
        msg.data = json.dumps(payload, separators=(",", ":"))
        self.time_sync_sent[self.time_sync_sequence] = float(host_send_us)
        self.time_sync_request_pub.publish(msg)
        self.time_sync_sequence += 1

    def on_time_sync_echo(self, msg: String):
        try:
            payload = json.loads(msg.data)
        except json.JSONDecodeError:
            self.get_logger().warning(f"Ignoring invalid time sync echo JSON: {msg.data}")
            return

        seq = int(payload.get("seq", -1))
        host_send_us = self.time_sync_sent.pop(seq, None)
        if host_send_us is None:
            return

        host_recv_us = float(time.monotonic_ns() // 1000)
        cm7_recv_us = float(payload.get("cm7_recv_us", 0.0))
        cm7_send_us = float(payload.get("cm7_send_us", 0.0))
        rtt_us = (host_recv_us - host_send_us) - max(0.0, cm7_send_us - cm7_recv_us)
        offset_us = ((host_send_us + host_recv_us) - (cm7_recv_us + cm7_send_us)) / 2.0

        candidate_rtt_ms = max(0.0, rtt_us) / 1000.0
        if self.time_sync_rtt_ms is None or candidate_rtt_ms <= self.time_sync_rtt_ms:
            self.clock_offset_us = offset_us
            self.time_sync_rtt_ms = candidate_rtt_ms
        self.time_sync_sample_count += 1

    def on_command(self, msg: String):
        try:
            data = json.loads(msg.data)
        except json.JSONDecodeError:
            self.get_logger().warning(f"Ignoring invalid command JSON: {msg.data}")
            return

        target_id = data.get("target_id")
        if target_id in (None, self.node_id, "all", "*"):
            self.get_logger().info(f"Observed dashboard command for node: {msg.data}")

    def on_timer(self):
        if self.last_heartbeat_time is None:
            return

        elapsed = time.monotonic() - self.last_heartbeat_time
        if elapsed < self.timeout_sec or self.offline_reported:
            return

        self.offline_reported = True
        self.publish_status(status="offline", health=0)
        self.publish_alert(
            level="warning",
            message=f"No heartbeat received for {elapsed:.1f}s"
        )

    def publish_status(self, status: str, health: int):
        now_monotonic = time.monotonic()
        heartbeat_metrics = self.topic_states["heartbeat"].as_dict(now_monotonic)
        position_metrics = self.topic_states["measured_position"].as_dict(now_monotonic)
        payload = {
            "id": self.node_id,
            "name": self.node_name,
            "status": status,
            "health": health,
            "uptime": self.format_uptime(),
            "type": self.node_type,
            "network": self.node_network,
            "metrics_summary": {
                "lag_ms": heartbeat_metrics.get("lag_ms", 0.0),
                "jitter_ms": heartbeat_metrics.get("jitter_ms", 0.0),
                "sync_ready": self.clock_offset_us is not None,
                "bandwidth_bps": round(
                    float(heartbeat_metrics.get("bandwidth_bps", 0.0)) +
                    float(position_metrics.get("bandwidth_bps", 0.0)),
                    3,
                ),
            },
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

    def publish_metrics(self):
        now_monotonic = time.monotonic()
        topic_metrics = {
            logical_topic: state.as_dict(now_monotonic)
            for logical_topic, state in self.topic_states.items()
        }
        aggregate = {
            "lag_ms": round(topic_metrics["heartbeat"]["lag_ms"], 3),
            "jitter_ms": round(topic_metrics["heartbeat"]["jitter_ms"], 3),
            "raw_delta_ms": round(topic_metrics["heartbeat"]["raw_delta_ms"], 3),
            "bandwidth_bps": round(
                float(topic_metrics["heartbeat"]["bandwidth_bps"]) +
                float(topic_metrics["measured_position"]["bandwidth_bps"]),
                3,
            ),
            "rate_hz": round(
                float(topic_metrics["heartbeat"]["rate_hz"]) +
                float(topic_metrics["measured_position"]["rate_hz"]),
                3,
            ),
            "sync_ready": self.clock_offset_us is not None and self.time_sync_sample_count > 0,
            "clock_offset_ms": round(self.clock_offset_us / 1000.0, 3) if self.clock_offset_us is not None else None,
            "time_sync_rtt_ms": round(self.time_sync_rtt_ms, 3) if self.time_sync_rtt_ms is not None else None,
            "time_sync_samples": self.time_sync_sample_count,
        }

        payload = {
            "node_id": self.node_id,
            "node_name": self.node_name,
            "timestamp": utc_timestamp(),
            "aggregate": aggregate,
            "topics": topic_metrics,
        }
        msg = String()
        msg.data = json.dumps(payload)
        self.metrics_pub.publish(msg)

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
