#!/usr/bin/env python3
"""ARM main-board ROS bridge (host side, sim == hardware path).

Upstream (from agent, real or simulated firmware):
  /heartbeat       std_msgs/Int32            CM7 liveness counter
  /joint_states    std_msgs/Float32MultiArray[18]  pos/vel/eff blocks of 6
  /joint_commands  std_msgs/Float32MultiArray     host -> CM7

Published here:
  /joint_states_norm  sensor_msgs/JointState  J1..J6 names+positions
  /imu/data_raw       sensor_msgs/Imu         synth until firmware IMU lands
  /diagnostics        diagnostic_msgs/DiagnosticArray  rails/brakes/fuse

Run: ROS_DOMAIN_ID=0 python3 arm_board_bridge.py  (needs rclpy, e.g. RoboStack)
Sim: firmware in Renode + uros-agent:8888 (see ../scripts/run_sim.sh --tap).
"""
import math
import time

try:
    import rclpy
    from rclpy.node import Node
    from rclpy.qos import QoSProfile, ReliabilityPolicy
    from std_msgs.msg import Float32MultiArray, Int32
    from sensor_msgs.msg import Imu, JointState
    from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus, KeyValue
    HAVE_RCLPY = True
except ImportError:  # documented degrade: topic map still valid, node won't run
    HAVE_RCLPY = False

JOINT_NAMES = ["J1", "J2", "J3", "J4", "J5", "J6"]
RATE_HZ = 10.0


def decode_joint_states(data):
    """Float32MultiArray[18] -> (pos[6], vel[6], eff[6]); tolerant to short arrays."""
    vals = list(data) + [0.0] * 18
    return vals[0:6], vals[6:12], vals[12:18]


if HAVE_RCLPY:
    class ArmBoardBridge(Node):
        def __init__(self):
            super().__init__("arm_board_bridge")
            qos = QoSProfile(depth=10, reliability=ReliabilityPolicy.BEST_EFFORT)
            self.js_pub = self.create_publisher(JointState, "/joint_states_norm", 10)
            self.imu_pub = self.create_publisher(Imu, "/imu/data_raw", 10)
            self.diag_pub = self.create_publisher(DiagnosticArray, "/diagnostics", 10)
            self.create_subscription(Float32MultiArray, "/joint_states", self.on_states, qos)
            self.create_subscription(Int32, "/heartbeat", self.on_hb, 10)
            self.last_hb = -1
            self.last_js_t = 0.0
            self.create_timer(1.0 / RATE_HZ, self.tick)
            self.get_logger().info("arm_board_bridge up (sim==hardware map)")

        def on_states(self, msg):
            pos, vel, eff = decode_joint_states(msg.data)
            out = JointState()
            out.header.stamp = self.get_clock().now().to_msg()
            out.name = JOINT_NAMES
            out.position = [float(p) for p in pos]
            out.velocity = [float(v) for v in vel]
            out.effort = [float(e) for e in eff]
            self.js_pub.publish(out)
            self.last_js_t = time.monotonic()

        def on_hb(self, msg):
            self.last_hb = int(msg.data)

        def tick(self):
            now = time.monotonic()
            # synth IMU until firmware publishes natively (see docs/pcb_elements.md §4)
            imu = Imu()
            imu.header.stamp = self.get_clock().now().to_msg()
            imu.header.frame_id = "imu1_link"
            imu.angular_velocity.z = 0.0
            imu.linear_acceleration.z = 9.801
            self.imu_pub.publish(imu)
            # diagnostics: agent freshness + rails/brakes static-OK (sim faults via model hooks)
            js_age = now - self.last_js_t if self.last_js_t else 1e9
            diag = DiagnosticArray()
            diag.header.stamp = self.get_clock().now().to_msg()
            diag.status = [
                DiagnosticStatus(level=DiagnosticStatus.OK if self.last_hb >= 0 else DiagnosticStatus.WARN,
                                 name="arm_board:agent_link",
                                 message=f"heartbeat={self.last_hb}",
                                 values=[KeyValue(key="js_age_s", value=f"{js_age:.2f}")]),
                DiagnosticStatus(level=DiagnosticStatus.OK, name="arm_board:rails_24v_5v",
                                 message="rails nominal (sim)",
                                 values=[KeyValue(key="fuse", value="intact")]),
                DiagnosticStatus(level=DiagnosticStatus.OK, name="arm_board:brakes_j2_j3",
                                 message="software state; see shared brake_state",
                                 values=[KeyValue(key="j2", value="released-on-motion"),
                                         KeyValue(key="j3", value="released-on-motion")]),
            ]
            self.diag_pub.publish(diag)

    def main():
        rclpy.init()
        node = ArmBoardBridge()
        try:
            rclpy.spin(node)
        finally:
            node.destroy_node()
            rclpy.shutdown()

    if __name__ == "__main__":
        main()
