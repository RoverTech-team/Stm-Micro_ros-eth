import os
import time
import json
import socket
import subprocess
from pathlib import Path
from typing import Generator, Optional, Dict, Any
from dataclasses import dataclass

import pytest
import requests
import docker
from docker.errors import DockerException, NotFound

# ========================================
# Real-time Logging Configuration
# ========================================

import logging
from datetime import datetime

# Create logs directory
LOGS_DIR = Path(__file__).parent / "logs"
LOGS_DIR.mkdir(exist_ok=True)

# Generate log filename with timestamp
LOG_TIMESTAMP = datetime.now().strftime("%Y%m%d_%H%M%S")
LOG_FILE = LOGS_DIR / f"e2e_debug_{LOG_TIMESTAMP}.log"


class RealTimeFileHandler(logging.FileHandler):
    """FileHandler that flushes after each log entry for real-time logging."""

    def emit(self, record):
        super().emit(record)
        self.flush()


def setup_e2e_logging():
    """Configure logging for e2e tests with real-time file output."""

    # Create formatter
    formatter = logging.Formatter(
        "%(asctime)s.%(msecs)03d [%(levelname)8s] %(name)s:%(lineno)d - %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )

    # Real-time file handler
    file_handler = RealTimeFileHandler(str(LOG_FILE), mode="w", encoding="utf-8")
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(formatter)

    # Console handler for errors only
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.WARNING)
    console_handler.setFormatter(formatter)

    # Root logger configuration
    root_logger = logging.getLogger()
    root_logger.setLevel(logging.DEBUG)
    root_logger.addHandler(file_handler)
    root_logger.addHandler(console_handler)

    # Set specific loggers for our test modules
    test_loggers = [
        "tests.e2e",
        "tests.e2e.test_xrcedds_transport",
        "tests.e2e.test_e2e_pipeline",
        "tests.simulation.renode.ethernet",
        "tests.simulation.renode.network_bridge",
    ]

    for logger_name in test_loggers:
        logger = logging.getLogger(logger_name)
        logger.setLevel(logging.DEBUG)
        logger.propagate = True

    # Reduce noise from third-party libraries
    logging.getLogger("urllib3").setLevel(logging.WARNING)
    logging.getLogger("docker").setLevel(logging.WARNING)
    logging.getLogger("requests").setLevel(logging.WARNING)

    return file_handler


# Initialize logging immediately
_log_handler = setup_e2e_logging()
_logger = logging.getLogger(__name__)
_logger.info(f"E2E logging initialized - log file: {LOG_FILE}")


ROS2_AVAILABLE = None


def _check_ros2_available() -> bool:
    try:
        import rclpy
        from rclpy.node import Node as RosNode
        from std_msgs.msg import String
        from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

        return True
    except ImportError:
        return False


def is_ros2_available() -> bool:
    global ROS2_AVAILABLE
    if ROS2_AVAILABLE is None:
        ROS2_AVAILABLE = _check_ros2_available()
    return ROS2_AVAILABLE


DOCKER_AVAILABLE = False
try:
    docker_client = docker.from_env()
    docker_client.ping()
    DOCKER_AVAILABLE = True
except (DockerException, Exception):
    pass


def is_running_in_docker() -> bool:
    if os.environ.get("RUNNING_IN_DOCKER", "").lower() in ("1", "true", "yes"):
        return True
    cgroup_path = Path("/proc/self/cgroup")
    if cgroup_path.exists():
        try:
            cgroup_content = cgroup_path.read_text()
            if "docker" in cgroup_content or "containerd" in cgroup_content:
                return True
        except Exception:
            pass
    dockerenv_path = Path("/.dockerenv")
    if dockerenv_path.exists():
        return True
    return False


IN_DOCKER = is_running_in_docker()


@dataclass
class TestConfig:
    dashboard_port: int = 5050
    agent_port: int = 8888
    startup_timeout: int = 60
    health_check_interval: int = 2
    health_check_retries: int = 30
    admin_username: str = "admin"
    admin_password: str = "testpass123"
    compose_project_name: str = "microk3_e2e_test"
    dashboard_host: str = "localhost"
    agent_host: str = "localhost"


@pytest.fixture(scope="session")
def test_config() -> TestConfig:
    if IN_DOCKER:
        dashboard_host = (
            os.environ.get("DASHBOARD_URL", "http://microk3:5050")
            .replace("http://", "")
            .split(":")[0]
        )
        if not dashboard_host or dashboard_host == "":
            dashboard_host = "microk3"
        agent_host = os.environ.get("AGENT_HOST", "uros-agent")
    else:
        dashboard_host = "localhost"
        agent_host = "localhost"

    return TestConfig(
        dashboard_port=int(os.environ.get("DASHBOARD_PORT", "5050")),
        agent_port=int(os.environ.get("AGENT_PORT", "8888")),
        startup_timeout=int(os.environ.get("STARTUP_TIMEOUT", "60")),
        health_check_interval=int(os.environ.get("HEALTH_CHECK_INTERVAL", "2")),
        health_check_retries=int(os.environ.get("HEALTH_CHECK_RETRIES", "30")),
        admin_username=os.environ.get("ADMIN_USERNAME", "admin"),
        admin_password=os.environ.get("ADMIN_PASSWORD", "testpass123"),
        compose_project_name=os.environ.get("COMPOSE_PROJECT_NAME", "microk3_e2e_test"),
        dashboard_host=dashboard_host,
        agent_host=agent_host,
    )


def wait_for_healthy(url: str, timeout: int = 60, interval: int = 2) -> bool:
    start_time = time.time()
    while time.time() - start_time < timeout:
        try:
            response = requests.get(url, timeout=5)
            if response.status_code == 200:
                data = response.json()
                if data.get("status") == "healthy":
                    return True
        except (requests.RequestException, json.JSONDecodeError):
            pass
        time.sleep(interval)
    return False


def wait_for_agent(host: str, port: int, timeout: int = 30) -> bool:
    start_time = time.time()
    while time.time() - start_time < timeout:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.settimeout(2)
            sock.sendto(b"", (host, port))
            sock.close()
            return True
        except (socket.error, OSError):
            pass
        time.sleep(1)
    return False


def check_container_health(container_name: str) -> bool:
    try:
        container = docker_client.containers.get(container_name)
        status = container.status.lower()
        if "running" not in status:
            return False
        container.reload()
        health = container.attrs.get("State", {}).get("Health", {})
        if health and health.get("Status", "").lower() == "healthy":
            return True
        return True
    except NotFound:
        return False
    except Exception:
        return False


def check_existing_stack() -> bool:
    microk3_healthy = check_container_health("microk3")
    uros_healthy = check_container_health("uros-agent")
    return microk3_healthy and uros_healthy


@pytest.fixture(scope="session")
def docker_compose_stack(test_config: TestConfig):
    if IN_DOCKER:
        dashboard_url = (
            f"http://{test_config.dashboard_host}:{test_config.dashboard_port}"
        )
        if wait_for_healthy(f"{dashboard_url}/health", timeout=30, interval=2):
            yield {
                "dashboard_url": dashboard_url,
                "agent_host": test_config.agent_host,
                "agent_port": test_config.agent_port,
            }
            return
        pytest.fail(f"Could not connect to microk3 at {dashboard_url}")

    if not DOCKER_AVAILABLE:
        pytest.skip("Docker not available")

    project_root = Path(__file__).parent.parent.parent
    microk3_dir = project_root / "microk3"
    compose_file = microk3_dir / "docker-compose.yml"

    if not compose_file.exists():
        pytest.skip(f"docker-compose.yml not found at {compose_file}")

    dashboard_url = f"http://localhost:{test_config.dashboard_port}"
    started_stack = False

    if check_existing_stack():
        try:
            if wait_for_healthy(f"{dashboard_url}/health", timeout=10, interval=1):
                yield {
                    "dashboard_url": dashboard_url,
                    "agent_host": "localhost",
                    "agent_port": test_config.agent_port,
                }
                return
        except Exception:
            pass

    started_stack = True
    env = os.environ.copy()
    env.update(
        {
            "COMPOSE_PROJECT_NAME": test_config.compose_project_name,
            "FLASK_PORT": str(test_config.dashboard_port),
            "ADMIN_USERNAME": test_config.admin_username,
            "ADMIN_PASSWORD": test_config.admin_password,
            "SECRET_KEY": "e2e-test-secret-key-do-not-use-in-production",
        }
    )

    compose_cmd = [
        "docker",
        "compose",
        "-f",
        str(compose_file),
        "-p",
        test_config.compose_project_name,
        "up",
        "-d",
        "--build",
    ]

    try:
        subprocess.run(
            compose_cmd, env=env, check=True, capture_output=True, cwd=str(microk3_dir)
        )
    except subprocess.CalledProcessError as e:
        pytest.skip(f"Failed to start Docker Compose stack: {e.stderr.decode()}")

    if not wait_for_healthy(f"{dashboard_url}/health", test_config.startup_timeout):
        subprocess.run(
            ["docker", "compose", "-p", test_config.compose_project_name, "logs"],
            capture_output=True,
            cwd=str(microk3_dir),
        )
        pytest.fail(
            f"MicroK3 stack did not become healthy within {test_config.startup_timeout}s"
        )

    yield {
        "dashboard_url": dashboard_url,
        "agent_host": "localhost",
        "agent_port": test_config.agent_port,
    }

    if started_stack:
        subprocess.run(
            [
                "docker",
                "compose",
                "-p",
                test_config.compose_project_name,
                "down",
                "-v",
                "--remove-orphans",
            ],
            capture_output=True,
            cwd=str(microk3_dir),
        )


@pytest.fixture
def dashboard_client(docker_compose_stack: Dict[str, Any], test_config: TestConfig):
    base_url = docker_compose_stack["dashboard_url"]

    class DashboardClient:
        def __init__(self, base_url: str, auth: Optional[tuple] = None):
            self.base_url = base_url.rstrip("/")
            self.auth = auth
            self.session = requests.Session()
            if auth:
                self.session.auth = auth

        def get(self, path: str, **kwargs) -> requests.Response:
            return self.session.get(f"{self.base_url}{path}", **kwargs)

        def post(self, path: str, **kwargs) -> requests.Response:
            return self.session.post(f"{self.base_url}{path}", **kwargs)

        def health_check(self) -> Dict[str, Any]:
            response = self.get("/health")
            response.raise_for_status()
            return response.json()

        def get_nodes(self) -> list:
            response = self.get("/api/nodes")
            response.raise_for_status()
            return response.json()

        def get_node(self, node_id: int) -> Dict[str, Any]:
            response = self.get(f"/api/nodes/{node_id}")
            response.raise_for_status()
            return response.json()

        def get_system_status(self) -> Dict[str, Any]:
            response = self.get("/api/system_status")
            response.raise_for_status()
            return response.json()

        def get_failures(self) -> list:
            response = self.get("/api/failures")
            response.raise_for_status()
            return response.json()

        def update_node(self, node_id: int, **kwargs) -> Dict[str, Any]:
            data = {"node_id": node_id, **kwargs}
            response = self.post(
                "/api/update_node",
                json=data,
                headers={"Content-Type": "application/json"},
            )
            response.raise_for_status()
            return response.json()

    return DashboardClient(
        base_url, auth=(test_config.admin_username, test_config.admin_password)
    )


@pytest.fixture
def mock_stm32_client(docker_compose_stack: Dict[str, Any], test_config: TestConfig):
    from .mock_stm32_client import MockSTM32Client

    client = MockSTM32Client(
        agent_host=docker_compose_stack["agent_host"],
        agent_port=docker_compose_stack["agent_port"],
        node_id=100,
        use_ros2=is_ros2_available(),
    )

    yield client

    try:
        client.disconnect()
    except Exception:
        pass


@pytest.fixture
def mock_stm32_client_udp(
    docker_compose_stack: Dict[str, Any], test_config: TestConfig
):
    from .mock_stm32_client import MockSTM32Client

    client = MockSTM32Client(
        agent_host=docker_compose_stack["agent_host"],
        agent_port=docker_compose_stack["agent_port"],
        node_id=200,
        use_ros2=False,
    )

    yield client

    try:
        client.disconnect()
    except Exception:
        pass


@pytest.fixture
def ros2_node():
    if not is_ros2_available():
        pytest.skip("ROS2/rclpy not available")

    import rclpy

    if not rclpy.ok():
        rclpy.init()

    node = rclpy.create_node("e2e_test_node")
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
def ros2_publisher(ros2_node):
    if not is_ros2_available():
        pytest.skip("ROS2/rclpy not available")

    from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy
    from std_msgs.msg import String

    qos = QoSProfile(
        reliability=ReliabilityPolicy.RELIABLE,
        durability=DurabilityPolicy.VOLATILE,
        depth=10,
    )

    status_pub = ros2_node.create_publisher(String, "microk3/node_status", qos)
    alert_pub = ros2_node.create_publisher(String, "microk3/system_alerts", qos)
    cmd_sub = ros2_node.create_subscription(
        String, "microk3/commands", lambda msg: None, qos
    )

    yield {
        "status_publisher": status_pub,
        "alert_publisher": alert_pub,
        "command_subscriber": cmd_sub,
        "node": ros2_node,
    }


def is_root_available() -> bool:
    """Check if running with root/sudo privileges."""
    return os.geteuid() == 0


ROOT_AVAILABLE = is_root_available()


def is_tap_available() -> bool:
    """Check if TAP tests can run (root + TAP scripts exist)."""
    if not ROOT_AVAILABLE:
        return False
    setup_script = (
        Path(__file__).parent.parent / "simulation" / "scripts" / "setup_tap.sh"
    )
    return setup_script.exists()


TAP_AVAILABLE = is_tap_available()


def pytest_configure(config):
    _logger.info("=" * 60)
    _logger.info("Pytest configuration starting")
    _logger.info("=" * 60)

    config.addinivalue_line("markers", "requires_docker: mark test as requiring Docker")
    config.addinivalue_line(
        "markers", "requires_ros2: mark test as requiring ROS2 environment"
    )
    config.addinivalue_line("markers", "e2e: mark test as end-to-end test")
    config.addinivalue_line("markers", "slow: mark test as slow running")
    config.addinivalue_line("markers", "tap: mark test as requiring TAP interface")
    config.addinivalue_line(
        "markers", "requires_root: mark test as requiring root privileges"
    )

    _logger.info("Registered markers:")
    _logger.info("  - requires_docker: mark test as requiring Docker")
    _logger.info("  - requires_ros2: mark test as requiring ROS2 environment")
    _logger.info("  - e2e: mark test as end-to-end test")
    _logger.info("  - slow: mark test as slow running")
    _logger.info("  - tap: mark test as requiring TAP interface")
    _logger.info("  - requires_root: mark test as requiring root privileges")
    _logger.info("Pytest configuration complete")


def pytest_collection_modifyitems(config, items):
    skip_docker = pytest.mark.skip(reason="Docker not available")
    skip_ros2 = pytest.mark.skip(reason="ROS2 environment not available")
    skip_root = pytest.mark.skip(reason="Root privileges required (run with sudo)")
    skip_tap = pytest.mark.skip(
        reason="TAP tests not available (requires root and TAP scripts)"
    )

    tap_enabled = config.getoption("--tap", default=False)
    if not tap_enabled:
        tap_enabled = os.environ.get("ENABLE_TAP_TESTS", "").lower() in (
            "1",
            "true",
            "yes",
        )

    for item in items:
        if "requires_docker" in item.keywords and not DOCKER_AVAILABLE:
            item.add_marker(skip_docker)
        if "requires_ros2" in item.keywords and not is_ros2_available():
            item.add_marker(skip_ros2)
        if "requires_root" in item.keywords and not ROOT_AVAILABLE:
            item.add_marker(skip_root)
        if "tap" in item.keywords and not tap_enabled:
            item.add_marker(skip_tap)


def pytest_addoption(parser):
    parser.addoption(
        "--tap",
        action="store_true",
        default=False,
        help="Enable TAP-based Renode simulation tests (requires root/sudo)",
    )
