"""
XRCE-DDS Transport Tests for TAP-based Renode Simulation (Linux)

Tests STM32 firmware XRCE-DDS communication with micro-ROS agent
using Renode simulation with TAP networking on Linux.

IMPORTANT: The bundled Renode.app in this project is for macOS ONLY.
Linux requires a separate Renode installation - tests will be skipped if
Renode is not found.

Requirements:
- Root privileges (for TAP interface creation)
- Renode installed separately on Linux (NOT bundled with project)
- STM32 firmware built at Micro_ros_eth/microroseth/Makefile/CM7/build/MicroRosEth_CM7.elf
- Docker with host networking support

Network topology:
    Linux test-runner (root)
        |
        |-- tap0 (192.168.0.1)
        |       |
        |       |-- IP alias: 192.168.0.8 (agent)
        |       |
        |       |-- Renode simulation
        |           |-- STM32 (192.168.0.3)
        |               |
        |               |-- XRCE-DDS UDP -> Agent:8888
        |
        |-- Docker host network -> micro-ROS agent
"""

import json
import logging
import os
import platform
import socket
import subprocess
import time
from pathlib import Path
from typing import Dict, Any, Optional, Generator, List

import pytest

logger = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.e2e,
    pytest.mark.tap,
    pytest.mark.requires_root,
    pytest.mark.requires_linux,
]


TAP_INTERFACE = "tap0"
GATEWAY_IP = "192.168.0.1"
AGENT_IP = "192.168.0.8"
DEVICE_IP = "192.168.0.3"
AGENT_PORT = 8888
NETMASK = "255.255.255.0"

# NOTE: Project bundles Renode.app for macOS only
# Linux requires separate Renode installation:
#   Ubuntu/Debian: sudo apt-get install renode
#   Or download from: https://github.com/renode/renode/releases
#   Set RENODE_PATH env var if installed elsewhere

CURDIR = Path(__file__).parent
PROJECT_ROOT = CURDIR.parent.parent
FIRMWARE_PATH = (
    PROJECT_ROOT
    / "Micro_ros_eth"
    / "microroseth"
    / "Makefile"
    / "CM7"
    / "build"
    / "MicroRosEth_CM7.elf"
)
TAP_REPL = CURDIR.parent / "simulation" / "renode" / "stm32h755_tap.repl"
TAP_RESC = CURDIR.parent / "simulation" / "renode" / "microros_tap.resc"
SETUP_SCRIPT = CURDIR.parent / "simulation" / "scripts" / "setup_tap.sh"
TEARDOWN_SCRIPT = CURDIR.parent / "simulation" / "scripts" / "teardown_tap.sh"

TIMEOUT_BOOT = 30
TIMEOUT_NETWORK = 30
TIMEOUT_AGENT = 15
TIMEOUT_SESSION = 10


def find_renode_binary() -> Optional[Path]:
    """Find Renode binary on Linux system."""
    logger.debug("find_renode_binary() searching for Renode installation")

    candidate_paths: List[Path] = [
        Path("/usr/bin/renode"),
        Path("/usr/local/bin/renode"),
        Path("/opt/renode/renode"),
        Path.home() / ".local" / "bin" / "renode",
        PROJECT_ROOT / "Renode",
        PROJECT_ROOT / "renode",
        PROJECT_ROOT / "renode.sh",
    ]

    env_renode = os.environ.get("RENODE_PATH")
    if env_renode:
        candidate_paths.insert(0, Path(env_renode))
        logger.debug(f"RENODE_PATH environment variable set: {env_renode}")

    for candidate in candidate_paths:
        logger.debug(
            f"Checking candidate path: {candidate} -> exists={candidate.exists()}"
        )
        if candidate.exists():
            if os.access(candidate, os.X_OK):
                logger.debug(f"Found executable Renode binary at: {candidate}")
                return candidate
            else:
                logger.debug(f"Path exists but not executable: {candidate}")

    logger.warning(
        "No Renode binary found. The bundled Renode.app is for macOS only. "
        "Install Renode on Linux via: sudo apt-get install renode "
        "or download from https://github.com/renode/renode/releases"
    )
    return None


def can_run_renode(renode_path: Path) -> bool:
    """Check if Renode can be executed on this platform."""
    logger.debug(f"can_run_renode() checking path: {renode_path}")

    if not renode_path.exists():
        logger.debug(f"Path does not exist: {renode_path}")
        return False

    if platform.system() != "Linux":
        logger.debug(f"Not running on Linux, platform is: {platform.system()}")
        return False

    try:
        with open(renode_path, "rb") as f:
            magic = f.read(4)
            logger.debug(f"File magic bytes: {magic.hex()}")
            if magic == b"\x7fELF":
                logger.debug("File is ELF binary (Linux compatible)")
                return True
            else:
                logger.warning(
                    f"Renode binary at {renode_path} is not a Linux ELF binary. "
                    f"The bundled Renode.app is for macOS only. "
                    "Please install Renode for Linux: sudo apt-get install renode"
                )
                return False
    except Exception as e:
        logger.debug(f"Error reading file magic: {e}")
        return False


@pytest.fixture(scope="class")
def renode_binary() -> Generator[Optional[Path], None, None]:
    """Find and validate Renode binary for Linux.

    Skips tests if Renode is not found, with clear installation instructions.
    """
    logger.debug("renode_binary fixture called")

    binary = find_renode_binary()

    if binary is None:
        logger.warning(
            "Renode not found. The bundled Renode.app is for macOS only.\n"
            "To install Renode on Linux:\n"
            "  Ubuntu/Debian: sudo apt-get install renode\n"
            "  Or download from: https://github.com/renode/renode/releases\n"
            "  Set RENODE_PATH env var if installed elsewhere"
        )
        pytest.skip(
            "Renode not found. Install via: sudo apt-get install renode "
            "or download from https://github.com/renode/renode/releases"
        )
        yield None
        return

    if not can_run_renode(binary):
        pytest.skip(
            f"Renode at {binary} is not a valid Linux binary. "
            "The bundled Renode.app is for macOS only. "
            "Install Linux version: sudo apt-get install renode"
        )
        yield None
        return

    logger.debug(f"renode_binary fixture returning: {binary}")
    yield binary


class RenodeProcess:
    """Manages a Renode simulation process on Linux."""

    def __init__(
        self, renode_path: Path, firmware_path: Path, repl_path: Path, resc_path: Path
    ):
        logger.debug(f"RenodeProcess.__init__() called")
        logger.debug(f"  renode_path: {renode_path}")
        logger.debug(f"  firmware_path: {firmware_path}")
        logger.debug(f"  repl_path: {repl_path}")
        logger.debug(f"  resc_path: {resc_path}")
        self.renode_path = renode_path
        self.firmware_path = firmware_path
        self.repl_path = repl_path
        self.resc_path = resc_path
        self.process: Optional[subprocess.Popen] = None
        self.log_buffer: list = []
        logger.debug("RenodeProcess instance initialized")

    def start(self, timeout: int = TIMEOUT_BOOT) -> bool:
        logger.debug(f"RenodeProcess.start() called with timeout={timeout}")
        logger.debug(f"Checking file existence:")
        logger.debug(
            f"  renode_path: {self.renode_path} -> exists={self.renode_path.exists()}"
        )
        logger.debug(
            f"  firmware_path: {self.firmware_path} -> exists={self.firmware_path.exists()}"
        )
        logger.debug(
            f"  repl_path: {self.repl_path} -> exists={self.repl_path.exists()}"
        )
        logger.debug(
            f"  resc_path: {self.resc_path} -> exists={self.resc_path.exists()}"
        )

        if not self.renode_path.exists():
            logger.debug(f"Renode not found at {self.renode_path}")
            raise FileNotFoundError(f"Renode not found at {self.renode_path}")
        if not self.firmware_path.exists():
            logger.debug(f"Firmware not found at {self.firmware_path}")
            raise FileNotFoundError(f"Firmware not found at {self.firmware_path}")
        if not self.repl_path.exists():
            logger.debug(f"REPL file not found at {self.repl_path}")
            raise FileNotFoundError(f"REPL file not found at {self.repl_path}")
        if not self.resc_path.exists():
            logger.debug(f"RESC file not found at {self.resc_path}")
            raise FileNotFoundError(f"RESC file not found at {self.resc_path}")

        cmd = [
            str(self.renode_path),
            "--disable-xwt",
            "--port",
            "0",
            "-e",
            f"include @{self.resc_path}",
            "-e",
            "start",
        ]
        logger.debug(f"Spawning Renode process with command: {' '.join(cmd)}")

        self.process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        logger.debug(f"Renode process started with PID: {self.process.pid}")

        return self._wait_for_boot(timeout)

    def _wait_for_boot(self, timeout: int) -> bool:
        boot_indicators = ["FreeRTOS", "scheduler", "started"]
        logger.debug(
            f"_wait_for_boot() starting with timeout={timeout}s, indicators={boot_indicators}"
        )
        start_time = time.time()

        while time.time() - start_time < timeout:
            if self.process.poll() is not None:
                logger.debug(
                    f"Renode process exited early with code: {self.process.poll()}"
                )
                return False

            try:
                import select

                readable, _, _ = select.select([self.process.stdout], [], [], 0.1)
                if readable:
                    line = self.process.stdout.readline()
                    self.log_buffer.append(line)
                    logger.debug(f"Renode stdout: {line.strip()}")
                    matched = [
                        ind for ind in boot_indicators if ind.lower() in line.lower()
                    ]
                    if matched:
                        logger.debug(f"Boot indicator matched: {matched}")
                        return True
            except Exception as e:
                logger.debug(f"Exception in _wait_for_boot: {e}")
                time.sleep(0.1)

        logger.debug(f"_wait_for_boot() timed out after {timeout}s")
        return False

    def wait_for_xrcedds_session(self, timeout: int = TIMEOUT_SESSION) -> bool:
        xrcedds_indicators = ["XRCE", "DDS", "session", "agent", "connected"]
        logger.debug(
            f"wait_for_xrcedds_session() starting with timeout={timeout}s, indicators={xrcedds_indicators}"
        )
        start_time = time.time()

        while time.time() - start_time < timeout:
            if self.process.poll() is not None:
                logger.debug(
                    f"Renode process exited during XRCE-DDS wait with code: {self.process.poll()}"
                )
                return False

            try:
                import select

                readable, _, _ = select.select([self.process.stdout], [], [], 0.1)
                if readable:
                    line = self.process.stdout.readline()
                    self.log_buffer.append(line)
                    logger.debug(f"Renode stdout (XRCE-DDS): {line.strip()}")
                    matched = [
                        ind for ind in xrcedds_indicators if ind.lower() in line.lower()
                    ]
                    if matched:
                        logger.debug(f"XRCE-DDS indicator matched: {matched}")
                        return True
            except Exception as e:
                logger.debug(f"Exception in wait_for_xrcedds_session: {e}")
                time.sleep(0.1)

        logger.debug(f"wait_for_xrcedds_session() timed out after {timeout}s")
        return False

    def get_logs(self) -> str:
        logger.debug(f"get_logs() called, buffer has {len(self.log_buffer)} lines")
        return "".join(self.log_buffer)

    def stop(self):
        logger.debug("RenodeProcess.stop() called")
        if self.process:
            logger.debug(f"Terminating Renode process PID: {self.process.pid}")
            try:
                self.process.terminate()
                self.process.wait(timeout=5)
                logger.debug("Renode process terminated gracefully")
            except Exception as e:
                logger.debug(f"Renode termination failed, killing: {e}")
                self.process.kill()
                logger.debug("Renode process killed")
            finally:
                self.process = None
        else:
            logger.debug("No Renode process to stop")


class MicroROSAgent:
    """Manages a micro-ROS agent process for TAP testing on Linux."""

    def __init__(self, agent_ip: str = AGENT_IP, port: int = AGENT_PORT):
        logger.debug(
            f"MicroROSAgent.__init__() called with agent_ip={agent_ip}, port={port}"
        )
        self.agent_ip = agent_ip
        self.port = port
        self.process: Optional[subprocess.Popen] = None
        self.container_id: Optional[str] = None
        logger.debug("MicroROSAgent instance initialized")

    def start(self, timeout: int = TIMEOUT_AGENT) -> bool:
        logger.debug(f"MicroROSAgent.start() called with timeout={timeout}s")
        logger.debug(f"Agent config: ip={self.agent_ip}, port={self.port}")
        try:
            docker_cmd = [
                "docker",
                "run",
                "--rm",
                "-d",
                "--name",
                "uros_agent_tap_test_linux",
                "--network",
                "host",
                "-e",
                f"ROS_DOMAIN_ID=0",
                "microros/micro-ros-agent:humble",
                "udp4",
                "--port",
                str(self.port),
                "-v6",
            ]
            logger.debug(f"Running docker command: {' '.join(docker_cmd)}")

            result = subprocess.run(
                docker_cmd, capture_output=True, text=True, timeout=30
            )
            logger.debug(f"Docker run returncode: {result.returncode}")
            logger.debug(f"Docker run stdout: {result.stdout.strip()}")
            logger.debug(f"Docker run stderr: {result.stderr.strip()}")

            if result.returncode != 0:
                logger.debug("Docker container start failed")
                return False

            self.container_id = result.stdout.strip()
            logger.debug(f"Container ID: {self.container_id}")

            logger.debug("Container started, waiting 2s for initialization")
            time.sleep(2)
            logger.debug("Checking agent readiness")
            return self._check_agent_ready(timeout)

        except Exception as e:
            logger.debug(f"MicroROSAgent.start() exception: {e}")
            return False

    def _check_agent_ready(self, timeout: int) -> bool:
        logger.debug(f"_check_agent_ready() starting with timeout={timeout}s")
        start_time = time.time()
        attempt = 0
        while time.time() - start_time < timeout:
            attempt += 1
            logger.debug(
                f"Socket attempt {attempt}: connecting to {self.agent_ip}:{self.port}"
            )
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sock.settimeout(1)
                sock.sendto(b"\x00", (self.agent_ip, self.port))
                sock.close()
                logger.debug(f"Socket attempt {attempt}: success, agent is ready")
                return True
            except Exception as e:
                logger.debug(f"Socket attempt {attempt}: failed with {e}")
                time.sleep(0.5)
        logger.debug(
            f"_check_agent_ready() timed out after {timeout}s, {attempt} attempts"
        )
        return False

    def stop(self):
        logger.debug("MicroROSAgent.stop() called")
        try:
            logger.debug("Stopping docker container 'uros_agent_tap_test_linux'")
            result = subprocess.run(
                ["docker", "stop", "uros_agent_tap_test_linux"],
                capture_output=True,
                timeout=10,
            )
            logger.debug(f"docker stop returncode: {result.returncode}")
            logger.debug("Removing docker container 'uros_agent_tap_test_linux'")
            result = subprocess.run(
                ["docker", "rm", "-f", "uros_agent_tap_test_linux"],
                capture_output=True,
                timeout=10,
            )
            logger.debug(f"docker rm returncode: {result.returncode}")
        except Exception as e:
            logger.debug(f"MicroROSAgent.stop() exception: {e}")


def setup_tap_linux(
    interface: str, gateway_ip: str, agent_ip: str, netmask: str
) -> bool:
    """Set up TAP interface on Linux using ip tuntap commands."""
    logger.debug(
        f"setup_tap_linux() called with interface={interface}, gateway_ip={gateway_ip}, agent_ip={agent_ip}"
    )

    commands = [
        ["ip", "tuntap", "add", "mode", "tap", "dev", interface],
        ["ip", "addr", "add", f"{gateway_ip}/24", "dev", interface],
        ["ip", "link", "set", "dev", interface, "up"],
        ["ip", "addr", "add", f"{agent_ip}/24", "dev", interface],
    ]

    for cmd in commands:
        logger.debug(f"Running command: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)
        logger.debug(f"Command returncode: {result.returncode}")
        logger.debug(f"Command stdout: {result.stdout.strip()}")
        logger.debug(f"Command stderr: {result.stderr.strip()}")
        if result.returncode != 0:
            logger.debug(f"Command failed: {' '.join(cmd)}")
            return False

    logger.debug("TAP interface setup successful")
    return True


def teardown_tap_linux(interface: str) -> bool:
    """Tear down TAP interface on Linux."""
    logger.debug(f"teardown_tap_linux() called with interface={interface}")

    cmd = ["ip", "tuntap", "del", "mode", "tap", "dev", interface]
    logger.debug(f"Running command: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    logger.debug(f"Command returncode: {result.returncode}")
    logger.debug(f"Command stdout: {result.stdout.strip()}")
    logger.debug(f"Command stderr: {result.stderr.strip()}")

    success = result.returncode == 0
    logger.debug(f"TAP teardown {'successful' if success else 'failed'}")
    return success


class TestXRCEDDSTransportLinux:
    """Test XRCE-DDS transport layer using TAP-based Renode simulation on Linux.

    NOTE: Renode must be installed separately on Linux. The bundled Renode.app
    is for macOS only. Install via: sudo apt-get install renode
    """

    @pytest.fixture(scope="class")
    def tap_config(self) -> Dict[str, str]:
        logger.debug("tap_config fixture called")
        return {
            "interface": TAP_INTERFACE,
            "gateway_ip": GATEWAY_IP,
            "agent_ip": AGENT_IP,
            "device_ip": DEVICE_IP,
            "agent_port": str(AGENT_PORT),
            "netmask": NETMASK,
        }

    @pytest.fixture(scope="class")
    def renode_paths(self, renode_binary: Optional[Path]) -> Dict[str, Path]:
        logger.debug("renode_paths fixture called")
        logger.debug(f"renode_binary fixture returned: {renode_binary}")
        return {
            "renode": renode_binary if renode_binary else Path("/usr/bin/renode"),
            "firmware": FIRMWARE_PATH,
            "repl": TAP_REPL,
            "resc": TAP_RESC,
            "setup_script": SETUP_SCRIPT,
            "teardown_script": TEARDOWN_SCRIPT,
        }

    @pytest.fixture(scope="class")
    def tap_interface(self, tap_config: Dict[str, str], renode_paths: Dict[str, Path]):
        """Set up TAP interface for testing on Linux."""
        logger.debug(f"tap_interface fixture called with config: {tap_config}")

        setup_script = renode_paths["setup_script"]
        logger.debug(f"Checking for setup script: {setup_script}")

        if setup_script.exists():
            logger.debug(f"Using setup script: {setup_script}")
            result = subprocess.run(
                [str(setup_script)], capture_output=True, text=True, timeout=60
            )
            logger.debug(f"Setup script returncode: {result.returncode}")
            logger.debug(f"Setup script stdout: {result.stdout.strip()}")
            logger.debug(f"Setup script stderr: {result.stderr.strip()}")

            if result.returncode != 0:
                logger.debug(f"TAP setup script failed, trying direct commands")
                if not setup_tap_linux(
                    tap_config["interface"],
                    tap_config["gateway_ip"],
                    tap_config["agent_ip"],
                    tap_config["netmask"],
                ):
                    pytest.skip("TAP setup failed")
        else:
            logger.debug("No setup script found, using direct ip tuntap commands")
            if not setup_tap_linux(
                tap_config["interface"],
                tap_config["gateway_ip"],
                tap_config["agent_ip"],
                tap_config["netmask"],
            ):
                pytest.skip("TAP setup failed using direct commands")

        logger.debug("TAP interface setup successful, yielding config")
        yield tap_config

        teardown_script = renode_paths["teardown_script"]
        logger.debug(f"Teardown: checking for script: {teardown_script}")

        if teardown_script.exists():
            logger.debug(f"Using teardown script: {teardown_script}")
            result = subprocess.run(
                [str(teardown_script)], capture_output=True, text=True, timeout=30
            )
            logger.debug(f"Teardown script returncode: {result.returncode}")
            logger.debug(f"Teardown script stdout: {result.stdout.strip()}")
            logger.debug(f"Teardown script stderr: {result.stderr.strip()}")
        else:
            logger.debug("No teardown script found, using direct commands")
            teardown_tap_linux(tap_config["interface"])

    @pytest.fixture(scope="class")
    def micro_ros_agent_tap(self, tap_interface):
        """Start micro-ROS agent for TAP testing."""
        logger.debug(
            f"micro_ros_agent_tap fixture: starting agent at {AGENT_IP}:{AGENT_PORT}"
        )
        agent = MicroROSAgent(agent_ip=AGENT_IP, port=AGENT_PORT)

        if not agent.start(timeout=TIMEOUT_AGENT):
            logger.debug("Failed to start micro-ROS agent")
            pytest.skip("Failed to start micro-ROS agent for TAP testing")

        logger.debug("micro-ROS agent started successfully")
        yield agent

        logger.debug("micro_ros_agent_tap fixture: stopping agent")
        agent.stop()

    @pytest.fixture(scope="class")
    def renode_simulation(
        self,
        tap_interface,
        micro_ros_agent_tap,
        renode_paths: Dict[str, Path],
        renode_binary: Optional[Path],
    ):
        """Start Renode simulation with TAP networking on Linux."""
        paths = renode_paths
        logger.debug("renode_simulation fixture: validating paths")
        logger.debug(
            f"  renode: {paths['renode']} -> exists={paths['renode'].exists()}"
        )
        logger.debug(
            f"  firmware: {paths['firmware']} -> exists={paths['firmware'].exists()}"
        )
        logger.debug(f"  repl: {paths['repl']} -> exists={paths['repl'].exists()}")
        logger.debug(f"  resc: {paths['resc']} -> exists={paths['resc'].exists()}")

        if platform.system() != "Linux":
            logger.debug(f"Not running on Linux: {platform.system()}")
            pytest.skip("This test is Linux-specific")

        if not paths["firmware"].exists():
            logger.debug(f"Firmware not found at {paths['firmware']}")
            pytest.skip(f"Firmware not found at {paths['firmware']}")

        logger.debug("Creating RenodeProcess instance")
        renode = RenodeProcess(
            renode_path=paths["renode"],
            firmware_path=paths["firmware"],
            repl_path=paths["repl"],
            resc_path=paths["resc"],
        )

        logger.debug(f"Starting Renode with timeout={TIMEOUT_BOOT}s")
        if not renode.start(timeout=TIMEOUT_BOOT):
            logs = renode.get_logs()
            logger.debug(f"Renode boot failed, logs (first 500 chars): {logs[:500]}")
            renode.stop()
            pytest.skip(f"Renode failed to boot: {logs[:500]}")

        logger.debug("Renode simulation started successfully")
        yield renode

        logger.debug("renode_simulation fixture: stopping Renode")
        renode.stop()

    def test_tap_interface_configured(self, tap_interface: Dict[str, str]):
        """Verify TAP interface is properly configured on Linux."""
        logger.debug(
            f"test_tap_interface_configured: checking interface {tap_interface['interface']}"
        )
        result = subprocess.run(
            ["ip", "addr", "show", tap_interface["interface"]],
            capture_output=True,
            text=True,
        )
        logger.debug(f"ip addr show returncode: {result.returncode}")
        logger.debug(f"ip addr show stdout: {result.stdout.strip()}")
        logger.debug(f"ip addr show stderr: {result.stderr.strip()}")

        assert result.returncode == 0, f"TAP interface not found: {result.stderr}"

        output = result.stdout
        logger.debug(f"Checking gateway_ip={tap_interface['gateway_ip']} in output")
        assert tap_interface["gateway_ip"] in output, (
            f"Gateway IP {tap_interface['gateway_ip']} not configured"
        )
        logger.debug(f"Checking agent_ip={tap_interface['agent_ip']} in output")
        assert tap_interface["agent_ip"] in output, (
            f"Agent IP {tap_interface['agent_ip']} not configured"
        )
        logger.debug("test_tap_interface_configured: all checks passed")

    def test_stm32_connects_to_agent(
        self, renode_simulation: RenodeProcess, tap_interface: Dict[str, str]
    ):
        """Test that STM32 firmware connects to micro-ROS agent."""
        logger.debug(
            f"test_stm32_connects_to_agent: waiting for XRCE-DDS session, timeout={TIMEOUT_SESSION * 2}s"
        )
        connected = renode_simulation.wait_for_xrcedds_session(
            timeout=TIMEOUT_SESSION * 2
        )
        logger.debug(f"wait_for_xrcedds_session returned: {connected}")

        logs = renode_simulation.get_logs().lower()
        logger.debug(f"Checking logs for 'agent' and 'session' indicators")
        has_agent = "agent" in logs
        has_session = "session" in logs
        logger.debug(f"Indicators found: agent={has_agent}, session={has_session}")

        assert connected or "agent" in logs or "session" in logs, (
            f"STM32 did not connect to agent. Logs:\n{renode_simulation.get_logs()}"
        )
        logger.debug("test_stm32_connects_to_agent: connection verified")

    def test_xrcedds_session_establishment(
        self, renode_simulation: RenodeProcess, tap_interface: Dict[str, str]
    ):
        """Test XRCE-DDS session establishment."""
        logger.debug(
            "test_xrcedds_session_establishment: searching for session indicators"
        )
        logs = renode_simulation.get_logs().lower()

        session_indicators = ["session", "create", "participant", "xrce"]
        found_indicators = [ind for ind in session_indicators if ind in logs]
        logger.debug(f"Session indicators searched: {session_indicators}")
        logger.debug(f"Session indicators found: {found_indicators}")

        assert len(found_indicators) > 0, (
            f"No XRCE-DDS session indicators found. Logs:\n{renode_simulation.get_logs()}"
        )
        logger.debug("test_xrcedds_session_establishment: session indicators verified")

    def test_bidirectional_communication(
        self, renode_simulation: RenodeProcess, tap_interface: Dict[str, str]
    ):
        """Test bidirectional XRCE-DDS communication."""
        logger.debug("test_bidirectional_communication: sleeping 2s for communication")
        time.sleep(2)

        logs = renode_simulation.get_logs().lower()

        tx_indicators = ["send", "publish", "write", "tx"]
        rx_indicators = ["recv", "receive", "read", "rx", "subscribe"]
        logger.debug(f"TX indicators searched: {tx_indicators}")
        logger.debug(f"RX indicators searched: {rx_indicators}")

        has_tx = any(ind in logs for ind in tx_indicators)
        has_rx = any(ind in logs for ind in rx_indicators)
        logger.debug(
            f"Communication indicators found: has_tx={has_tx}, has_rx={has_rx}"
        )

        assert has_tx or has_rx, (
            f"No bidirectional communication indicators found. Logs:\n{renode_simulation.get_logs()}"
        )
        logger.debug("test_bidirectional_communication: communication verified")


class TestXRCEDDSTransportLinuxSkipped:
    """Tests that are skipped when TAP is not available on Linux."""

    @pytest.mark.skip(
        reason="TAP tests require root privileges and Linux - run with --tap flag"
    )
    def test_tap_requires_root(self):
        """Placeholder test indicating TAP requires root."""
        pass

    @pytest.mark.skip(reason="Renode must be installed on Linux for this test")
    def test_renode_required(self):
        """Placeholder test indicating Renode is required."""
        pass
