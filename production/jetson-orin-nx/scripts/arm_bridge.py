#!/usr/bin/env python3
"""Arm bridge: translates between STM32 micro-ROS topics and ros2_control
TopicBasedSystem topics.

STM32 side (Float32MultiArray, flat packed [pos*N, vel*N, eff*N]):
  subscribes: ARM_STATES_INPUT_TOPIC    (default: /joint_states)
  publishes:  ARM_COMMANDS_OUTPUT_TOPIC (default: /joint_commands)

ros2_control side (sensor_msgs/JointState):
  publishes:  ARM_STATES_OUTPUT_TOPIC   (default: /hardware_level/states)
  subscribes: ARM_COMMANDS_INPUT_TOPIC  (default: /hardware_level/commands)
"""

import os

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from std_msgs.msg import Float32MultiArray

JOINT_NAMES = ["joint_1", "joint_2", "joint_3"]
N = len(JOINT_NAMES)

STATES_INPUT_TOPIC = os.environ.get("ARM_STATES_INPUT_TOPIC", "/joint_states")
COMMANDS_OUTPUT_TOPIC = os.environ.get("ARM_COMMANDS_OUTPUT_TOPIC", "/joint_commands")
STATES_OUTPUT_TOPIC = os.environ.get(
    "ARM_STATES_OUTPUT_TOPIC", "/hardware_level/states"
)
COMMANDS_INPUT_TOPIC = os.environ.get(
    "ARM_COMMANDS_INPUT_TOPIC", "/hardware_level/commands"
)


class ArmBridge(Node):
    def __init__(self):
        super().__init__("arm_bridge")
        self.get_logger().info(
            f"Arm bridge starting:\n"
            f"  {STATES_INPUT_TOPIC} (Float32MultiArray) -> {STATES_OUTPUT_TOPIC} (JointState)\n"
            f"  {COMMANDS_INPUT_TOPIC} (JointState) -> {COMMANDS_OUTPUT_TOPIC} (Float32MultiArray)"
        )
        # STM32 -> ros2_control
        self.pub_states = self.create_publisher(JointState, STATES_OUTPUT_TOPIC, 10)
        self.sub_states = self.create_subscription(
            Float32MultiArray, STATES_INPUT_TOPIC, self.on_states, 10
        )
        # ros2_control -> STM32
        self.pub_cmd = self.create_publisher(
            Float32MultiArray, COMMANDS_OUTPUT_TOPIC, 10
        )
        self.sub_cmd = self.create_subscription(
            JointState, COMMANDS_INPUT_TOPIC, self.on_commands, 10
        )

    def on_states(self, msg: Float32MultiArray):
        d = msg.data
        if len(d) < 3 * N:
            self.get_logger().warn(
                f"Expected {3 * N} floats in {STATES_INPUT_TOPIC}, got {len(d)} — skipping"
            )
            return
        js = JointState()
        js.header.stamp = self.get_clock().now().to_msg()
        js.name = JOINT_NAMES
        js.position = list(d[0:N])
        js.velocity = list(d[N : 2 * N])
        js.effort = list(d[2 * N : 3 * N])
        self.pub_states.publish(js)

    def on_commands(self, msg: JointState):
        idx = {name: i for i, name in enumerate(msg.name)}
        missing = [n for n in JOINT_NAMES if n not in idx]
        if missing:
            self.get_logger().warn(
                f"Command message missing joints {missing} — skipping"
            )
            return
        pos = [msg.position[idx[n]] if msg.position else 0.0 for n in JOINT_NAMES]
        vel = [msg.velocity[idx[n]] if msg.velocity else 0.0 for n in JOINT_NAMES]
        eff = [msg.effort[idx[n]] if msg.effort else 0.0 for n in JOINT_NAMES]
        out = Float32MultiArray()
        out.data = pos + vel + eff
        self.pub_cmd.publish(out)


def main():
    rclpy.init()
    node = ArmBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
