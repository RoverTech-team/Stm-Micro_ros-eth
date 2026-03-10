import os
import socket
import subprocess
import time
import asyncio
import pytest
from typing import Generator, Optional
from dataclasses import dataclass

try:
    import rclpy
    from rclpy.node import Node
    from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

    ROS2_AVAILABLE = True
except ImportError:
    ROS2_AVAILABLE = False


@dataclass
class TestConfig:
    agent_ip: str
    agent_port: int
    client_port: int
    discovery_timeout_ms: int
    communication_timeout_ms: int
    max_retries: int
    retry_delay_ms: int
    large_message_size: int


@pytest.fixture(scope="session")
def test_config() -> TestConfig:
    return TestConfig(
        agent_ip=os.environ.get("MICRO_ROS_AGENT_IP", "192.168.1.100"),
        agent_port=int(os.environ.get("MICRO_ROS_AGENT_PORT", "8888")),
        client_port=int(os.environ.get("MICRO_ROS_CLIENT_PORT", "8889")),
        discovery_timeout_ms=int(os.environ.get("DISCOVERY_TIMEOUT_MS", "5000")),
        communication_timeout_ms=int(
            os.environ.get("COMMUNICATION_TIMEOUT_MS", "3000")
        ),
        max_retries=int(os.environ.get("MAX_RETRIES", "3")),
        retry_delay_ms=int(os.environ.get("RETRY_DELAY_MS", "1000")),
        large_message_size=int(os.environ.get("LARGE_MESSAGE_SIZE", "65536")),
    )


@pytest.fixture(scope="session")
def micro_ros_agent(test_config: TestConfig):
    agent_process = None
    agent_available = False

    agent_command = os.environ.get("MICRO_ROS_AGENT_CMD", "micro-ros-agent")

    try:
        result = subprocess.run(
            ["which", agent_command], capture_output=True, timeout=5
        )
        agent_available = result.returncode == 0
    except (subprocess.TimeoutExpired, FileNotFoundError):
        agent_available = False

    if agent_available:
        try:
            agent_process = subprocess.Popen(
                [agent_command, "udp4", "--port", str(test_config.agent_port), "-v6"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                preexec_fn=os.setsid if os.name != "nt" else None,
            )
            time.sleep(2)

            if agent_process.poll() is not None:
                agent_available = False
                agent_process = None
        except Exception:
            agent_available = False
            agent_process = None

    yield {
        "process": agent_process,
        "available": agent_available,
        "ip": test_config.agent_ip,
        "port": test_config.agent_port,
    }

    if agent_process:
        try:
            if os.name != "nt":
                os.killpg(os.getpgid(agent_process.pid), subprocess.signal.SIGTERM)
            else:
                agent_process.terminate()
            agent_process.wait(timeout=5)
        except Exception:
            if agent_process:
                agent_process.kill()


@pytest.fixture
def udp_client(test_config: TestConfig):
    client_socket = None

    def create_socket():
        nonlocal client_socket
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        client_socket.settimeout(test_config.communication_timeout_ms / 1000.0)
        return client_socket

    yield create_socket

    if client_socket:
        try:
            client_socket.close()
        except Exception:
            pass


@pytest.fixture
def dds_participant(test_config: TestConfig):
    if not ROS2_AVAILABLE:
        pytest.skip("ROS2/rclpy not available")

    rclpy.init()
    node = rclpy.create_node("test_dds_participant", context=rclpy.Context())

    yield node

    try:
        node.destroy_node()
    except Exception:
        pass
    try:
        rclpy.shutdown()
    except Exception:
        pass


@pytest.fixture
def ros2_context():
    if not ROS2_AVAILABLE:
        pytest.skip("ROS2/rclpy not available")

    rclpy.init()
    yield rclpy.Context()

    try:
        rclpy.shutdown()
    except Exception:
        pass


@pytest.fixture
def ros2_node(ros2_context):
    if not ROS2_AVAILABLE:
        pytest.skip("ROS2/rclpy not available")

    node = rclpy.create_node("test_ros2_node", context=ros2_context)
    yield node

    try:
        node.destroy_node()
    except Exception:
        pass


@pytest.fixture
def qos_profiles():
    if not ROS2_AVAILABLE:
        pytest.skip("ROS2/rclpy not available")

    return {
        "reliable": QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            depth=10,
        ),
        "best_effort": QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            depth=10,
        ),
        "transient_local": QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            depth=10,
        ),
    }


def pytest_configure(config):
    config.addinivalue_line(
        "markers", "requires_hardware: mark test as requiring physical hardware"
    )
    config.addinivalue_line(
        "markers", "requires_agent: mark test as requiring micro-ROS agent"
    )
    config.addinivalue_line(
        "markers", "requires_ros2: mark test as requiring ROS2 environment"
    )
    config.addinivalue_line("markers", "slow: mark test as slow running")
    config.addinivalue_line("markers", "integration: mark test as integration test")


def pytest_collection_modifyitems(config, items):
    skip_hardware = pytest.mark.skip(reason="Hardware not available")
    skip_agent = pytest.mark.skip(reason="micro-ROS agent not available")
    skip_ros2 = pytest.mark.skip(reason="ROS2 environment not available")

    requires_agent = os.environ.get("REQUIRES_AGENT", "false").lower() == "true"
    has_hardware = os.environ.get("HARDWARE_AVAILABLE", "false").lower() == "true"

    for item in items:
        if "requires_hardware" in item.keywords and not has_hardware:
            item.add_marker(skip_hardware)
        if "requires_agent" in item.keywords and not requires_agent:
            item.add_marker(skip_agent)
        if "requires_ros2" in item.keywords and not ROS2_AVAILABLE:
            item.add_marker(skip_ros2)


@pytest.fixture(scope="session")
def hardware_available() -> bool:
    return os.environ.get("HARDWARE_AVAILABLE", "false").lower() == "true"


@pytest.fixture(scope="session")
def agent_available(micro_ros_agent) -> bool:
    return micro_ros_agent["available"]


@pytest.fixture
def async_event_loop():
    loop = asyncio.new_event_loop()
    yield loop
    loop.close()


@pytest.fixture
async def async_udp_client(test_config: TestConfig):
    class AsyncUDPClient:
        def __init__(self):
            self.transport = None
            self.protocol = None

        async def connect(self, host: str, port: int):
            loop = asyncio.get_event_loop()
            self.transport, self.protocol = await asyncio.wait_for(
                loop.create_datagram_endpoint(
                    asyncio.DatagramProtocol, remote_addr=(host, port)
                ),
                timeout=test_config.communication_timeout_ms / 1000.0,
            )
            return self.transport, self.protocol

        async def send(self, data: bytes):
            if self.transport:
                self.transport.sendto(data)

        async def close(self):
            if self.transport:
                self.transport.close()

    client = AsyncUDPClient()
    yield client
    await client.close()


@pytest.fixture
def message_factory():
    if not ROS2_AVAILABLE:
        pytest.skip("ROS2/rclpy not available")

    try:
        from std_msgs.msg import String
        from geometry_msgs.msg import Twist
        from sensor_msgs.msg import Image

        return {
            "std_msgs/String": String,
            "geometry_msgs/Twist": Twist,
            "sensor_msgs/Image": Image,
        }
    except ImportError:
        pytest.skip("ROS2 message types not available")


@pytest.fixture
def wait_for_message():
    if not ROS2_AVAILABLE:
        pytest.skip("ROS2/rclpy not available")

    def _wait_for_message(node, topic_name, timeout_sec=5.0):
        received = {"data": None, "received": False}

        def callback(msg):
            received["data"] = msg
            received["received"] = True

        subscription = node.create_subscription(type(msg), topic_name, callback, 10)

        start_time = time.time()
        while not received["received"] and (time.time() - start_time) < timeout_sec:
            rclpy.spin_once(node, timeout_sec=0.1)

        node.destroy_subscription(subscription)
        return received["data"] if received["received"] else None

    return _wait_for_message
