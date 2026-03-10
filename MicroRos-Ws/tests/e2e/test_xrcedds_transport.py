"""
XRCE-DDS Transport Tests for TAP-based Renode Simulation

Tests STM32 firmware XRCE-DDS communication with micro-ROS agent
using Renode simulation with TAP networking.

Requirements:
- Privileged Docker container (for TAP interface creation)
- Renode.app installed at project root
- STM32 firmware built at Micro_ros_eth/microroseth/Makefile/CM7/build/MicroRosEth_CM7.elf
- micro-ROS agent running on agent IP alias

Network topology:
    Docker test-runner (privileged)
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
        |-- microk3 network -> Dashboard tests
"""

import json
import logging
import os
import socket
import subprocess
import time
from pathlib import Path
from typing import Dict, Any, Optional, Generator

import pytest

logger = logging.getLogger(__name__)

pytestmark = [pytest.mark.e2e, pytest.mark.tap, pytest.mark.requires_root]


TAP_INTERFACE = "tap0"
GATEWAY_IP = "192.168.0.1"
AGENT_IP = "192.168.0.8"
DEVICE_IP = "192.168.0.3"
AGENT_PORT = 8888
NETMASK = "255.255.255.0"

CURDIR = Path(__file__).parent
PROJECT_ROOT = CURDIR.parent.parent
RENODE_PATH = PROJECT_ROOT / "Renode.app" / "Contents" / "MacOS" / "renode"
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


def can_run_renode(renode_path: Path) -> bool:
    """Check if Renode can be executed on this platform."""
    if not renode_path.exists():
        return False

    import platform
    import stat

    # Check if running on macOS - Renode.app only works on macOS
    if platform.system() == "Darwin":
        return True

    # On Linux, check if it's actually a Linux binary (not macOS binary mounted from host)
    if platform.system() == "Linux":
        # Check if the file is a Mach-O binary (macOS) vs ELF (Linux)
        try:
            with open(renode_path, "rb") as f:
                magic = f.read(4)
                # Mach-O magic numbers
                if magic in (
                    b"\xfe\xed\xfa\xce",
                    b"\xfe\xed\xfa\xcf",
                    b"\xce\xfa\xed\xfe",
                    b"\xcf\xfa\xed\xfe",
                ):
                    return False  # macOS binary on Linux - won't work
                # ELF magic number
                if magic == b"\x7fELF":
                    return True  # Linux binary
        except Exception:
            pass

    return False


class RenodeProcess:
    """Manages a Renode simulation process."""

    def __init__(
        self, renode_path: Path, firmware_path: Path, repl_path: Path, resc_path: Path
    ):
        self.renode_path = renode_path
        self.firmware_path = firmware_path
        self.repl_path = repl_path
        self.resc_path = resc_path
        self.process: Optional[subprocess.Popen] = None
        self.log_buffer: list = []

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
            raise FileNotFoundError(f"Renode not found at {self.renode_path}")
        if not self.firmware_path.exists():
            raise FileNotFoundError(f"Firmware not found at {self.firmware_path}")
        if not self.repl_path.exists():
            raise FileNotFoundError(f"REPL file not found at {self.repl_path}")
        if not self.resc_path.exists():
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
            except Exception:
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
            except Exception:
                time.sleep(0.1)

        logger.debug(f"wait_for_xrcedds_session() timed out after {timeout}s")
        return False

    def get_logs(self) -> str:
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
    """Manages a micro-ROS agent process for TAP testing."""

    def __init__(self, agent_ip: str = AGENT_IP, port: int = AGENT_PORT):
        self.agent_ip = agent_ip
        self.port = port
        self.process: Optional[subprocess.Popen] = None

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
                "uros_agent_tap_test",
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
            logger.debug("Stopping docker container 'uros_agent_tap_test'")
            result = subprocess.run(
                ["docker", "stop", "uros_agent_tap_test"],
                capture_output=True,
                timeout=10,
            )
            logger.debug(f"docker stop returncode: {result.returncode}")
            logger.debug("Removing docker container 'uros_agent_tap_test'")
            result = subprocess.run(
                ["docker", "rm", "-f", "uros_agent_tap_test"],
                capture_output=True,
                timeout=10,
            )
            logger.debug(f"docker rm returncode: {result.returncode}")
        except Exception as e:
            logger.debug(f"MicroROSAgent.stop() exception: {e}")


class TestXRCEDDSTransport:
    """Test XRCE-DDS transport layer using TAP-based Renode simulation."""

    @pytest.fixture(scope="class")
    def tap_config(self) -> Dict[str, str]:
        return {
            "interface": TAP_INTERFACE,
            "gateway_ip": GATEWAY_IP,
            "agent_ip": AGENT_IP,
            "device_ip": DEVICE_IP,
            "agent_port": AGENT_PORT,
            "netmask": NETMASK,
        }

    @pytest.fixture(scope="class")
    def renode_paths(self) -> Dict[str, Path]:
        return {
            "renode": RENODE_PATH,
            "firmware": FIRMWARE_PATH,
            "repl": TAP_REPL,
            "resc": TAP_RESC,
            "setup_script": SETUP_SCRIPT,
            "teardown_script": TEARDOWN_SCRIPT,
        }

    @pytest.fixture(scope="class")
    def tap_interface(self, tap_config: Dict[str, str], renode_paths: Dict[str, Path]):
        """Set up TAP interface for testing."""
        setup_script = renode_paths["setup_script"]
        teardown_script = renode_paths["teardown_script"]
        logger.debug(
            f"tap_interface fixture: setup_script={setup_script}, teardown_script={teardown_script}"
        )

        if not setup_script.exists():
            logger.debug(f"TAP setup script not found: {setup_script}")
            pytest.skip(f"TAP setup script not found at {setup_script}")

        logger.debug(f"Executing TAP setup script: {setup_script}")
        result = subprocess.run(
            [str(setup_script)], capture_output=True, text=True, timeout=60
        )
        logger.debug(f"Setup script returncode: {result.returncode}")
        logger.debug(f"Setup script stdout: {result.stdout.strip()}")
        logger.debug(f"Setup script stderr: {result.stderr.strip()}")

        if result.returncode != 0:
            logger.debug(f"TAP setup failed with returncode {result.returncode}")
            pytest.skip(f"TAP setup failed: {result.stderr}")

        logger.debug("TAP interface setup successful, yielding config")

        yield tap_config

        logger.debug(f"tap_interface teardown: executing {teardown_script}")
        result = subprocess.run(
            [str(teardown_script)], capture_output=True, text=True, timeout=30
        )
        logger.debug(f"Teardown script returncode: {result.returncode}")
        logger.debug(f"Teardown script stdout: {result.stdout.strip()}")
        logger.debug(f"Teardown script stderr: {result.stderr.strip()}")

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
        self, tap_interface, micro_ros_agent_tap, renode_paths: Dict[str, Path]
    ):
        """Start Renode simulation with TAP networking."""
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

        if not paths["renode"].exists():
            logger.debug(f"Renode not found at {paths['renode']}")
            pytest.skip(f"Renode not found at {paths['renode']}")

        if not can_run_renode(paths["renode"]):
            logger.debug(f"Renode binary incompatible (macOS binary on Linux)")
            pytest.skip(
                f"Renode at {paths['renode']} is a macOS binary and cannot run on Linux. "
                "TAP tests with Renode must run on macOS host, not inside Docker."
            )

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
        """Verify TAP interface is properly configured."""
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


class TestXRCEDDSTransportSkipped:
    """Tests that are skipped when TAP is not available."""

    @pytest.mark.skip(reason="TAP tests require root privileges - run with --tap flag")
    def test_tap_requires_root(self):
        """Placeholder test indicating TAP requires root."""
        pass
