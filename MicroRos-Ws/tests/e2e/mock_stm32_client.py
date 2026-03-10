import json
import socket
import threading
import time
from typing import Optional, Callable, Dict, Any, List
from dataclasses import dataclass
from enum import Enum

try:
    import rclpy
    from rclpy.node import Node as RosNode
    from std_msgs.msg import String
    from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

    ROS2_AVAILABLE = True
except ImportError:
    ROS2_AVAILABLE = False


class NodeStatus(Enum):
    ACTIVE = "active"
    STANDBY = "standby"
    OFFLINE = "offline"
    ERROR = "error"


@dataclass
class NodeState:
    id: int
    status: str
    health: int
    uptime: str
    name: Optional[str] = None


class MockSTM32Client:
    def __init__(
        self,
        agent_host: str = "localhost",
        agent_port: int = 8888,
        node_id: int = 1,
        node_name: str = None,
        use_ros2: bool = True,
    ):
        self.agent_host = agent_host
        self.agent_port = agent_port
        self.node_id = node_id
        self.node_name = node_name or f"STM32_Node_{node_id}"
        self.use_ros2 = use_ros2 and ROS2_AVAILABLE

        self._state = NodeState(
            id=node_id,
            status=NodeStatus.ACTIVE.value,
            health=100,
            uptime="0s",
            name=self.node_name,
        )
        self._connected = False
        self._running = False
        self._start_time = None
        self._thread: Optional[threading.Thread] = None
        self._command_callbacks: List[Callable] = []
        self._received_commands: List[Dict[str, Any]] = []

        self._ros_node: Optional[RosNode] = None
        self._ros_executor = None
        self._ros_thread: Optional[threading.Thread] = None
        self._status_publisher = None
        self._alert_publisher = None
        self._command_subscription = None

        self._udp_socket: Optional[socket.socket] = None
        self._udp_thread: Optional[threading.Thread] = None

    def connect(self) -> bool:
        if self._connected:
            return True

        if self.use_ros2:
            return self._connect_ros2()
        else:
            return self._connect_udp()

    def _connect_ros2(self) -> bool:
        try:
            if not rclpy.ok():
                rclpy.init()

            self._ros_node = rclpy.create_node(f"mock_stm32_{self.node_id}")

            qos = QoSProfile(
                reliability=ReliabilityPolicy.RELIABLE,
                durability=DurabilityPolicy.VOLATILE,
                depth=10,
            )

            self._status_publisher = self._ros_node.create_publisher(
                String, "microk3/node_status", qos
            )
            self._alert_publisher = self._ros_node.create_publisher(
                String, "microk3/system_alerts", qos
            )
            self._command_subscription = self._ros_node.create_subscription(
                String, "microk3/commands", self._command_callback, qos
            )

            from rclpy.executors import MultiThreadedExecutor

            self._ros_executor = MultiThreadedExecutor()
            self._ros_executor.add_node(self._ros_node)

            self._ros_thread = threading.Thread(
                target=self._ros_executor.spin, daemon=True
            )
            self._ros_thread.start()

            self._connected = True
            self._running = True
            self._start_time = time.time()
            return True

        except Exception as e:
            print(f"ROS2 connection failed: {e}")
            return False

    def _connect_udp(self) -> bool:
        try:
            self._udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self._udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._udp_socket.setblocking(False)
            self._udp_socket.bind(("0.0.0.0", 0))

            self._udp_thread = threading.Thread(target=self._udp_listen, daemon=True)
            self._udp_thread.start()

            self._connected = True
            self._running = True
            self._start_time = time.time()
            return True

        except Exception as e:
            print(f"UDP connection failed: {e}")
            return False

    def disconnect(self):
        self._running = False
        self._connected = False

        if self._ros_executor:
            try:
                self._ros_executor.shutdown()
            except Exception:
                pass

        if self._ros_node:
            try:
                self._ros_node.destroy_node()
            except Exception:
                pass

        if self._udp_socket:
            try:
                self._udp_socket.close()
            except Exception:
                pass

        self._ros_node = None
        self._ros_executor = None
        self._udp_socket = None

    def _command_callback(self, msg):
        try:
            data = json.loads(msg.data)
            self._received_commands.append(data)
            for callback in self._command_callbacks:
                callback(data)
        except json.JSONDecodeError:
            pass

    def _udp_listen(self):
        while self._running:
            try:
                data, addr = self._udp_socket.recvfrom(4096)
                try:
                    parsed = json.loads(data.decode())
                    self._received_commands.append(parsed)
                    for callback in self._command_callbacks:
                        callback(parsed)
                except (json.JSONDecodeError, UnicodeDecodeError):
                    pass
            except BlockingIOError:
                time.sleep(0.1)
            except Exception:
                break

    def publish_status(
        self, status: str = None, health: int = None, uptime: str = None
    ):
        if not self._connected:
            raise RuntimeError("Client not connected")

        if status:
            self._state.status = status
        if health is not None:
            self._state.health = health
        if uptime:
            self._state.uptime = uptime
        else:
            elapsed = int(time.time() - self._start_time) if self._start_time else 0
            hours, remainder = divmod(elapsed, 3600)
            minutes, seconds = divmod(remainder, 60)
            self._state.uptime = f"{hours}h {minutes}m {seconds}s"

        message = {
            "id": self.node_id,
            "status": self._state.status,
            "health": self._state.health,
            "uptime": self._state.uptime,
            "name": self.node_name,
        }

        if self.use_ros2 and self._status_publisher:
            msg = String()
            msg.data = json.dumps(message)
            self._status_publisher.publish(msg)
        else:
            self._send_udp_message("microk3/node_status", message)

        return message

    def send_alert(self, message: str, level: str = "warning"):
        if not self._connected:
            raise RuntimeError("Client not connected")

        alert_data = {
            "node_id": self.node_id,
            "msg": message,
            "level": level,
            "timestamp": time.time(),
        }

        if self.use_ros2 and self._alert_publisher:
            msg = String()
            msg.data = json.dumps(alert_data)
            self._alert_publisher.publish(msg)
        else:
            self._send_udp_message("microk3/system_alerts", alert_data)

        return alert_data

    def _send_udp_message(self, topic: str, data: Dict[str, Any]):
        if self._udp_socket:
            packet = json.dumps({"topic": topic, "data": data})
            self._udp_socket.sendto(packet.encode(), (self.agent_host, self.agent_port))

    def set_status(self, status: str):
        if status not in [s.value for s in NodeStatus]:
            raise ValueError(
                f"Invalid status: {status}. Must be one of {[s.value for s in NodeStatus]}"
            )
        self._state.status = status
        return self.publish_status()

    def set_health(self, health: int):
        if not 0 <= health <= 100:
            raise ValueError("Health must be between 0 and 100")
        self._state.health = health
        return self.publish_status()

    def simulate_active(self):
        return self.set_status(NodeStatus.ACTIVE.value)

    def simulate_standby(self):
        return self.set_status(NodeStatus.STANDBY.value)

    def simulate_error(self, error_message: str = "Simulated error"):
        self.set_status(NodeStatus.ERROR.value)
        return self.send_alert(error_message, level="error")

    def simulate_offline(self):
        self._state.status = NodeStatus.OFFLINE.value
        return self.disconnect()

    def add_command_callback(self, callback: Callable[[Dict[str, Any]], None]):
        self._command_callbacks.append(callback)

    def get_received_commands(self) -> List[Dict[str, Any]]:
        return self._received_commands.copy()

    def clear_received_commands(self):
        self._received_commands.clear()

    @property
    def connected(self) -> bool:
        return self._connected

    @property
    def state(self) -> NodeState:
        return self._state

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()
        return False
