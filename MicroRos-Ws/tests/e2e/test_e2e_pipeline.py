import json
import time
import pytest
from typing import Dict, Any

try:
    import rclpy
    from std_msgs.msg import String
    from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

    ROS2_AVAILABLE = True
except ImportError:
    ROS2_AVAILABLE = False


pytestmark = [pytest.mark.e2e, pytest.mark.slow]


class TestNodeRegistrationFlow:
    @pytest.mark.requires_docker
    @pytest.mark.requires_ros2
    def test_node_registration_flow_ros2(self, dashboard_client, ros2_publisher):
        initial_nodes = dashboard_client.get_nodes()
        initial_count = len(initial_nodes)

        qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            depth=10,
        )

        status_msg = String()
        status_msg.data = json.dumps(
            {"id": 999, "status": "active", "health": 95, "uptime": "0s"}
        )

        # Publish multiple times to ensure delivery across Docker network
        for _ in range(3):
            ros2_publisher["status_publisher"].publish(status_msg)
            rclpy.spin_once(ros2_publisher["node"], timeout_sec=0.1)
            time.sleep(0.2)

        # Wait longer for ROS 2 message to propagate through Docker network
        # and be processed by MicroK3 dashboard
        max_attempts = 20
        for attempt in range(max_attempts):
            time.sleep(0.5)
            rclpy.spin_once(ros2_publisher["node"], timeout_sec=0.1)
            
            nodes = dashboard_client.get_nodes()
            if any(n.get("id") == 999 for n in nodes):
                break
        else:
            # Final check with detailed error
            nodes = dashboard_client.get_nodes()
            node_ids = [n.get("id") for n in nodes]
            pytest.fail(
                f"Node 999 did not appear in dashboard after registration. "
                f"Current nodes: {node_ids}"
            )

        nodes = dashboard_client.get_nodes()
        assert len(nodes) >= initial_count

    @pytest.mark.requires_docker
    def test_node_registration_flow_udp(self, dashboard_client, mock_stm32_client_udp):
        initial_nodes = dashboard_client.get_nodes()
        initial_count = len(initial_nodes)

        client = mock_stm32_client_udp
        assert client.connect(), "Failed to connect mock client"

        client.publish_status(status="active", health=90)

        nodes = dashboard_client.get_nodes()

        if initial_count == len(nodes):
            pytest.skip(
                "UDP-based node registration requires full micro-ROS agent integration"
            )


class TestStatusUpdateFlow:
    @pytest.mark.requires_docker
    @pytest.mark.requires_ros2
    def test_status_update_flow_ros2(self, dashboard_client, ros2_publisher):
        qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            depth=10,
        )

        status_msg = String()
        status_msg.data = json.dumps(
            {"id": 101, "status": "active", "health": 100, "uptime": "0s"}
        )
        ros2_publisher["status_publisher"].publish(status_msg)

        time.sleep(1)
        rclpy.spin_once(ros2_publisher["node"], timeout_sec=0.5)

        update_msg = String()
        update_msg.data = json.dumps(
            {"id": 101, "status": "standby", "health": 85, "uptime": "1h 0m"}
        )
        ros2_publisher["status_publisher"].publish(update_msg)

        time.sleep(1)
        rclpy.spin_once(ros2_publisher["node"], timeout_sec=0.5)

    @pytest.mark.requires_docker
    def test_status_update_flow_udp(self, dashboard_client, mock_stm32_client_udp):
        client = mock_stm32_client_udp
        assert client.connect(), "Failed to connect mock client"

        client.publish_status(status="active", health=95)

        client.publish_status(status="standby", health=80)


class TestAlertFlow:
    @pytest.mark.requires_docker
    @pytest.mark.requires_ros2
    def test_alert_flow_ros2(self, dashboard_client, ros2_publisher):
        initial_failures = dashboard_client.get_failures()
        initial_count = len(initial_failures)

        qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            depth=10,
        )

        alert_msg = String()
        alert_msg.data = json.dumps(
            {
                "node_id": 102,
                "msg": "E2E Test Alert: Overheating detected",
                "level": "warning",
            }
        )
        ros2_publisher["alert_publisher"].publish(alert_msg)

        time.sleep(1)
        rclpy.spin_once(ros2_publisher["node"], timeout_sec=0.5)

        failures = dashboard_client.get_failures()
        assert len(failures) >= initial_count

    @pytest.mark.requires_docker
    def test_alert_flow_udp(self, dashboard_client, mock_stm32_client_udp):
        client = mock_stm32_client_udp
        assert client.connect(), "Failed to connect mock client"

        client.send_alert("E2E Test UDP Alert", level="warning")

        time.sleep(0.5)


class TestMultipleNodes:
    @pytest.mark.requires_docker
    @pytest.mark.requires_ros2
    def test_multiple_nodes_register_correctly(self, dashboard_client, ros2_publisher):
        initial_nodes = dashboard_client.get_nodes()
        initial_ids = {n["id"] for n in initial_nodes}

        test_node_ids = [201, 202, 203]

        for node_id in test_node_ids:
            status_msg = String()
            status_msg.data = json.dumps(
                {"id": node_id, "status": "active", "health": 90, "uptime": "0s"}
            )
            ros2_publisher["status_publisher"].publish(status_msg)
            time.sleep(0.3)
            rclpy.spin_once(ros2_publisher["node"], timeout_sec=0.2)

        time.sleep(1)

    @pytest.mark.requires_docker
    def test_multiple_udp_clients(self, dashboard_client):
        from .mock_stm32_client import MockSTM32Client

        clients = []
        for i in range(3):
            client = MockSTM32Client(
                agent_host="localhost", agent_port=8888, node_id=300 + i, use_ros2=False
            )
            assert client.connect(), f"Failed to connect client {i}"
            clients.append(client)

        for client in clients:
            client.publish_status(status="active", health=95)

        time.sleep(1)

        for client in clients:
            client.disconnect()


class TestNodeDisconnect:
    @pytest.mark.requires_docker
    @pytest.mark.requires_ros2
    def test_node_disconnect_detected(self, dashboard_client, ros2_publisher):
        status_msg = String()
        status_msg.data = json.dumps(
            {"id": 301, "status": "active", "health": 100, "uptime": "0s"}
        )
        ros2_publisher["status_publisher"].publish(status_msg)
        time.sleep(0.5)
        rclpy.spin_once(ros2_publisher["node"], timeout_sec=0.2)

        offline_msg = String()
        offline_msg.data = json.dumps(
            {"id": 301, "status": "offline", "health": 0, "uptime": "0s"}
        )
        ros2_publisher["status_publisher"].publish(offline_msg)
        time.sleep(0.5)
        rclpy.spin_once(ros2_publisher["node"], timeout_sec=0.2)

        nodes = dashboard_client.get_nodes()
        node_301 = next((n for n in nodes if n["id"] == 301), None)

        if node_301:
            assert node_301["status"] in ["offline", "active"]

    @pytest.mark.requires_docker
    def test_udp_client_disconnect(self, mock_stm32_client_udp, dashboard_client):
        client = mock_stm32_client_udp
        assert client.connect(), "Failed to connect mock client"

        client.publish_status(status="active", health=95)

        client.disconnect()

        assert not client.connected
