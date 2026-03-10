import os
from pathlib import Path

CURDIR = Path(__file__).parent
PROJECT_ROOT = CURDIR.parent.parent.parent

TAP_INTERFACE = "tap0"
GATEWAY_IP = "192.168.0.1"
AGENT_IP = "192.168.0.8"
DEVICE_IP = "192.168.0.3"
AGENT_PORT = 8888
NETMASK = "255.255.255.0"

RENODE_PATH = str(PROJECT_ROOT / "Renode.app" / "Contents" / "MacOS" / "renode")
FIRMWARE_PATH = str(
    PROJECT_ROOT
    / "Micro_ros_eth"
    / "microroseth"
    / "Makefile"
    / "CM7"
    / "build"
    / "MicroRosEth_CM7.elf"
)
TAP_REPL = str(CURDIR.parent / "renode" / "stm32h755_tap.repl")
TAP_RESC = str(CURDIR.parent / "renode" / "microros_tap.resc")
SETUP_SCRIPT = str(CURDIR.parent / "scripts" / "setup_tap.sh")
TEARDOWN_SCRIPT = str(CURDIR.parent / "scripts" / "teardown_tap.sh")

SIM_DURATION = 60
TIMEOUT_BOOT = 30
TIMEOUT_NETWORK = 30
TIMEOUT_AGENT = 10

BOOT_INDICATORS = ["FreeRTOS", "scheduler", "started", "boot"]
NETWORK_INDICATORS = ["Ethernet", "ETH", "link", "UDP", "IP"]
XRCE_DDS_INDICATORS = [
    "XRCE",
    "DDS",
    "agent",
    "session",
    "topic",
    "publisher",
    "subscriber",
]


def get_tap_config() -> dict:
    return {
        "interface": TAP_INTERFACE,
        "gateway_ip": GATEWAY_IP,
        "agent_ip": AGENT_IP,
        "device_ip": DEVICE_IP,
        "agent_port": AGENT_PORT,
        "netmask": NETMASK,
    }


def get_paths() -> dict:
    return {
        "renode": RENODE_PATH,
        "firmware": FIRMWARE_PATH,
        "tap_repl": TAP_REPL,
        "tap_resc": TAP_RESC,
        "setup_script": SETUP_SCRIPT,
        "teardown_script": TEARDOWN_SCRIPT,
    }


def get_timeouts() -> dict:
    return {
        "sim_duration": SIM_DURATION,
        "boot": TIMEOUT_BOOT,
        "network": TIMEOUT_NETWORK,
        "agent": TIMEOUT_AGENT,
    }


def get_indicators() -> dict:
    return {
        "boot": BOOT_INDICATORS,
        "network": NETWORK_INDICATORS,
        "xrcedds": XRCE_DDS_INDICATORS,
    }


def validate_tap_config() -> dict:
    return {
        "valid": True,
        "interface": TAP_INTERFACE,
        "gateway": GATEWAY_IP,
        "agent_ip": AGENT_IP,
        "device_ip": DEVICE_IP,
        "netmask": NETMASK,
    }
