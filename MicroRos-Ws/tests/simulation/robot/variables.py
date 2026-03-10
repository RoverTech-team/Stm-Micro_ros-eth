import os
import random
import string
import hashlib
from pathlib import Path

CURDIR = Path(__file__).parent
PROJECT_ROOT = CURDIR.parent.parent.parent

RENODE_PATH = str(PROJECT_ROOT / "Renode.app" / "Contents" / "MacOS" / "renode")
RENODE_SCRIPT = str(CURDIR.parent / "renode" / "stm32h7_eth.resc")
FIRMWARE_PATH = str(PROJECT_ROOT / "build" / "stm32h7_eth.elf")

DEFAULT_IP = "192.168.1.100"
AGENT_IP = "192.168.1.10"
AGENT_PORT = 8888
TEST_UDP_PORT = 12345
DEFAULT_GATEWAY = "192.168.1.1"
DEFAULT_NETMASK = "255.255.255.0"
DEFAULT_DNS = "192.168.1.1"

TIMEOUT_BOOT = 30
TIMEOUT_UART = 10
TIMEOUT_NETWORK = 15
TIMEOUT_MICROROS = 20
TIMEOUT_UDP = 5

PROMPT = ">"
BOOT_INDICATOR = "FreeRTOS scheduler started"
ETH_LINK_UP = "Ethernet link up"
DHCP_SUCCESS = "DHCP: Got IP address"
MICROROS_CONNECTED = "micro-ROS agent connected"
MICROROS_DISCONNECTED = "micro-ROS agent disconnected"

MAX_UDP_PACKET_SIZE = 65507
DEFAULT_UDP_COUNT = 1
DEFAULT_UDP_DELAY = 0.1

MAX_TEST_DATA_SIZE = 4096
DEFAULT_TEST_DATA_SIZE = 100


def get_firmware_path(firmware_name: str = "stm32h7_eth.elf") -> str:
    return str(PROJECT_ROOT / "build" / firmware_name)


def get_renode_script(script_name: str = "stm32h7_eth.resc") -> str:
    return str(CURDIR.parent / "renode" / script_name)


def get_output_dir(test_name: str = "test") -> str:
    output_dir = CURDIR.parent / "results" / test_name
    output_dir.mkdir(parents=True, exist_ok=True)
    return str(output_dir)


def generate_test_data(size: int = DEFAULT_TEST_DATA_SIZE, seed: int = None) -> str:
    if seed is not None:
        random.seed(seed)
    chars = string.ascii_letters + string.digits
    return "".join(random.choices(chars, k=size))


def generate_binary_data(size: int = DEFAULT_TEST_DATA_SIZE, seed: int = None) -> bytes:
    if seed is not None:
        random.seed(seed)
    return bytes(random.randint(0, 255) for _ in range(size))


def generate_hex_data(size: int = DEFAULT_TEST_DATA_SIZE) -> str:
    return generate_binary_data(size).hex()


def calculate_checksum(data: str) -> str:
    return hashlib.md5(data.encode()).hexdigest()


def calculate_binary_checksum(data: bytes) -> str:
    return hashlib.md5(data).hexdigest()


def generate_ip_address(base: str = "192.168.1", host: int = None) -> str:
    if host is None:
        host = random.randint(2, 254)
    return f"{base}.{host}"


def generate_port(low: int = 1024, high: int = 65535) -> int:
    return random.randint(low, high)


def generate_topic_name(prefix: str = "topic") -> str:
    suffix = "".join(random.choices(string.ascii_lowercase + string.digits, k=8))
    return f"/{prefix}_{suffix}"


def generate_message_type() -> dict:
    message_types = [
        {"type": "std_msgs/msg/String", "fields": ["data"]},
        {"type": "std_msgs/msg/Int32", "fields": ["data"]},
        {"type": "std_msgs/msg/Float32", "fields": ["data"]},
        {"type": "geometry_msgs/msg/Twist", "fields": ["linear", "angular"]},
        {"type": "sensor_msgs/msg/Temperature", "fields": ["temperature", "variance"]},
    ]
    return random.choice(message_types)


def generate_dds_message(msg_type: str = None) -> dict:
    if msg_type is None:
        msg_info = generate_message_type()
        msg_type = msg_info["type"]

    messages = {
        "std_msgs/msg/String": {"data": generate_test_data(50)},
        "std_msgs/msg/Int32": {"data": random.randint(-2147483648, 2147483647)},
        "std_msgs/msg/Float32": {"data": random.uniform(-1000.0, 1000.0)},
        "geometry_msgs/msg/Twist": {
            "linear": {"x": 0.0, "y": 0.0, "z": 0.0},
            "angular": {"x": 0.0, "y": 0.0, "z": 0.0},
        },
        "sensor_msgs/msg/Temperature": {
            "temperature": random.uniform(-50.0, 150.0),
            "variance": random.uniform(0.0, 1.0),
        },
    }
    return messages.get(msg_type, {"data": generate_test_data(50)})


def get_test_config() -> dict:
    return {
        "renode": {
            "path": RENODE_PATH,
            "script": RENODE_SCRIPT,
        },
        "firmware": {
            "path": FIRMWARE_PATH,
        },
        "network": {
            "device_ip": DEFAULT_IP,
            "agent_ip": AGENT_IP,
            "agent_port": AGENT_PORT,
            "test_udp_port": TEST_UDP_PORT,
            "gateway": DEFAULT_GATEWAY,
            "netmask": DEFAULT_NETMASK,
            "dns": DEFAULT_DNS,
        },
        "timeouts": {
            "boot": TIMEOUT_BOOT,
            "uart": TIMEOUT_UART,
            "network": TIMEOUT_NETWORK,
            "microros": TIMEOUT_MICROROS,
            "udp": TIMEOUT_UDP,
        },
        "patterns": {
            "boot_indicator": BOOT_INDICATOR,
            "eth_link_up": ETH_LINK_UP,
            "dhcp_success": DHCP_SUCCESS,
            "microros_connected": MICROROS_CONNECTED,
            "microros_disconnected": MICROROS_DISCONNECTED,
        },
    }


def validate_network_config() -> dict:
    return {
        "valid": True,
        "ip": DEFAULT_IP,
        "gateway": DEFAULT_GATEWAY,
        "netmask": DEFAULT_NETMASK,
        "dns": DEFAULT_DNS,
    }
