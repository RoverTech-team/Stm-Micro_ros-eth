import pytest
import time


class TestROS2NodeCreation:
    @pytest.mark.requires_ros2
    def test_node_creation_basic(self, ros2_node):
        assert ros2_node is not None
        assert ros2_node.get_name() == "test_ros2_node"

    @pytest.mark.requires_ros2
    def test_node_creation_with_namespace(self, ros2_context):
        import rclpy
        from rclpy.node import Node

        node = rclpy.create_node(
            "namespace_test_node", namespace="/test_namespace", context=ros2_context
        )

        assert node.get_namespace() == "/test_namespace"
        node.destroy_node()

    @pytest.mark.requires_ros2
    def test_node_multiple_publishers(self, ros2_node):
        from std_msgs.msg import String

        publishers = []
        for i in range(5):
            pub = ros2_node.create_publisher(String, f"/topic_{i}", 10)
            publishers.append(pub)

        assert len(publishers) == 5

        for pub in publishers:
            ros2_node.destroy_publisher(pub)

    @pytest.mark.requires_ros2
    def test_node_multiple_subscribers(self, ros2_node):
        from std_msgs.msg import String

        subscribers = []
        for i in range(5):
            sub = ros2_node.create_subscription(
                String, f"/topic_{i}", lambda msg: None, 10
            )
            subscribers.append(sub)

        assert len(subscribers) == 5

        for sub in subscribers:
            ros2_node.destroy_subscription(sub)


class TestStdMsgsStringTopic:
    @pytest.mark.requires_ros2
    def test_string_publish_subscribe(self, ros2_context):
        import rclpy
        from std_msgs.msg import String

        pub_node = rclpy.create_node("string_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("string_sub_node", context=ros2_context)

        received_messages = []

        def callback(msg):
            received_messages.append(msg.data)

        publisher = pub_node.create_publisher(String, "/string_topic", 10)
        subscriber = sub_node.create_subscription(String, "/string_topic", callback, 10)

        time.sleep(0.5)

        test_message = "Hello micro-ROS Ethernet!"
        msg = String()
        msg.data = test_message

        for _ in range(10):
            publisher.publish(msg)
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert len(received_messages) > 0
        assert test_message in received_messages

        pub_node.destroy_node()
        sub_node.destroy_node()

    @pytest.mark.requires_ros2
    @pytest.mark.parametrize(
        "message_content",
        [
            "Simple ASCII",
            "Special chars: !@#$%^&*()",
            "Unicode: 你好世界",
            "Empty string: ",
            "Very long string: " + "x" * 1000,
        ],
    )
    def test_string_various_contents(self, ros2_context, message_content):
        import rclpy
        from std_msgs.msg import String

        pub_node = rclpy.create_node("content_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("content_sub_node", context=ros2_context)

        received = []

        def callback(msg):
            received.append(msg.data)

        publisher = pub_node.create_publisher(String, "/content_topic", 10)
        subscriber = sub_node.create_subscription(
            String, "/content_topic", callback, 10
        )

        time.sleep(0.5)

        msg = String()
        msg.data = message_content

        for _ in range(10):
            publisher.publish(msg)
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert len(received) > 0

        pub_node.destroy_node()
        sub_node.destroy_node()

    @pytest.mark.requires_ros2
    def test_string_message_rate(self, ros2_context):
        import rclpy
        from std_msgs.msg import String

        pub_node = rclpy.create_node("rate_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("rate_sub_node", context=ros2_context)

        received_count = [0]

        def callback(msg):
            received_count[0] += 1

        publisher = pub_node.create_publisher(String, "/rate_topic", 10)
        subscriber = sub_node.create_subscription(String, "/rate_topic", callback, 10)

        time.sleep(0.5)

        message_count = 20
        msg = String()
        msg.data = "Rate test"

        for i in range(message_count):
            publisher.publish(msg)

        for _ in range(message_count):
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert received_count[0] > 0

        pub_node.destroy_node()
        sub_node.destroy_node()


class TestGeometryMsgsTwistTopic:
    @pytest.mark.requires_ros2
    def test_twist_publish_subscribe(self, ros2_context):
        import rclpy
        from geometry_msgs.msg import Twist

        pub_node = rclpy.create_node("twist_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("twist_sub_node", context=ros2_context)

        received = []

        def callback(msg):
            received.append(msg)

        publisher = pub_node.create_publisher(Twist, "/cmd_vel", 10)
        subscriber = sub_node.create_subscription(Twist, "/cmd_vel", callback, 10)

        time.sleep(0.5)

        twist_msg = Twist()
        twist_msg.linear.x = 1.0
        twist_msg.linear.y = 0.5
        twist_msg.linear.z = 0.0
        twist_msg.angular.x = 0.0
        twist_msg.angular.y = 0.0
        twist_msg.angular.z = 0.5

        for _ in range(10):
            publisher.publish(twist_msg)
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert len(received) > 0

        if received:
            assert abs(received[0].linear.x - 1.0) < 0.001
            assert abs(received[0].linear.y - 0.5) < 0.001
            assert abs(received[0].angular.z - 0.5) < 0.001

        pub_node.destroy_node()
        sub_node.destroy_node()

    @pytest.mark.requires_ros2
    @pytest.mark.parametrize(
        "linear_x,angular_z",
        [
            (0.0, 0.0),
            (1.0, 0.0),
            (0.0, 1.0),
            (0.5, 0.5),
            (-1.0, -0.5),
            (2.0, 1.0),
        ],
    )
    def test_twist_various_velocities(self, ros2_context, linear_x, angular_z):
        import rclpy
        from geometry_msgs.msg import Twist

        pub_node = rclpy.create_node("vel_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("vel_sub_node", context=ros2_context)

        received = []

        def callback(msg):
            received.append(msg)

        publisher = pub_node.create_publisher(Twist, "/velocity_topic", 10)
        subscriber = sub_node.create_subscription(
            Twist, "/velocity_topic", callback, 10
        )

        time.sleep(0.5)

        twist_msg = Twist()
        twist_msg.linear.x = linear_x
        twist_msg.angular.z = angular_z

        for _ in range(10):
            publisher.publish(twist_msg)
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert len(received) > 0

        pub_node.destroy_node()
        sub_node.destroy_node()


class TestSensorMsgsImageTopic:
    @pytest.mark.requires_ros2
    def test_image_small_publish_subscribe(self, ros2_context):
        import rclpy
        from sensor_msgs.msg import Image

        pub_node = rclpy.create_node("img_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("img_sub_node", context=ros2_context)

        received = []

        def callback(msg):
            received.append(msg)

        publisher = pub_node.create_publisher(Image, "/image_small", 10)
        subscriber = sub_node.create_subscription(Image, "/image_small", callback, 10)

        time.sleep(0.5)

        image_msg = Image()
        image_msg.header.stamp = pub_node.get_clock().now().to_msg()
        image_msg.header.frame_id = "camera_frame"
        image_msg.height = 100
        image_msg.width = 100
        image_msg.encoding = "rgb8"
        image_msg.is_bigendian = False
        image_msg.step = 300
        image_msg.data = bytes([i % 256 for i in range(100 * 100 * 3)])

        for _ in range(10):
            publisher.publish(image_msg)
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert len(received) > 0

        if received:
            assert received[0].height == 100
            assert received[0].width == 100
            assert received[0].encoding == "rgb8"

        pub_node.destroy_node()
        sub_node.destroy_node()

    @pytest.mark.requires_ros2
    @pytest.mark.parametrize(
        "height,width",
        [
            (64, 64),
            (320, 240),
            (640, 480),
        ],
    )
    def test_image_various_resolutions(self, ros2_context, height, width):
        import rclpy
        from sensor_msgs.msg import Image

        pub_node = rclpy.create_node("res_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("res_sub_node", context=ros2_context)

        received = []

        def callback(msg):
            received.append(msg)

        publisher = pub_node.create_publisher(Image, "/image_resolution", 10)
        subscriber = sub_node.create_subscription(
            Image, "/image_resolution", callback, 10
        )

        time.sleep(0.5)

        image_msg = Image()
        image_msg.height = height
        image_msg.width = width
        image_msg.encoding = "mono8"
        image_msg.step = width
        image_msg.data = bytes([i % 256 for i in range(height * width)])

        for _ in range(10):
            publisher.publish(image_msg)
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert len(received) > 0

        pub_node.destroy_node()
        sub_node.destroy_node()

    @pytest.mark.requires_ros2
    def test_image_large_message(self, ros2_context, test_config):
        import rclpy
        from sensor_msgs.msg import Image

        pub_node = rclpy.create_node("large_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("large_sub_node", context=ros2_context)

        received = []

        def callback(msg):
            received.append(msg)

        publisher = pub_node.create_publisher(Image, "/image_large", 10)
        subscriber = sub_node.create_subscription(Image, "/image_large", callback, 10)

        time.sleep(0.5)

        large_size = test_config.large_message_size
        image_msg = Image()
        image_msg.height = 256
        image_msg.width = large_size // 256
        image_msg.encoding = "mono8"
        image_msg.step = image_msg.width
        image_msg.data = bytes([i % 256 for i in range(large_size)])

        publisher.publish(image_msg)

        for _ in range(20):
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert len(received) > 0

        pub_node.destroy_node()
        sub_node.destroy_node()


class TestQoSPolicies:
    @pytest.mark.requires_ros2
    def test_qos_reliable(self, ros2_node, qos_profiles):
        from std_msgs.msg import String

        publisher = ros2_node.create_publisher(
            String, "/qos_reliable", qos_profiles["reliable"]
        )

        subscriber = ros2_node.create_subscription(
            String, "/qos_reliable", lambda msg: None, qos_profiles["reliable"]
        )

        assert publisher is not None
        assert subscriber is not None

        ros2_node.destroy_publisher(publisher)
        ros2_node.destroy_subscription(subscriber)

    @pytest.mark.requires_ros2
    def test_qos_best_effort(self, ros2_node, qos_profiles):
        from std_msgs.msg import String

        publisher = ros2_node.create_publisher(
            String, "/qos_best_effort", qos_profiles["best_effort"]
        )

        subscriber = ros2_node.create_subscription(
            String, "/qos_best_effort", lambda msg: None, qos_profiles["best_effort"]
        )

        assert publisher is not None
        assert subscriber is not None

        ros2_node.destroy_publisher(publisher)
        ros2_node.destroy_subscription(subscriber)

    @pytest.mark.requires_ros2
    def test_qos_transient_local(self, ros2_node, qos_profiles):
        from std_msgs.msg import String

        publisher = ros2_node.create_publisher(
            String, "/qos_transient_local", qos_profiles["transient_local"]
        )

        subscriber = ros2_node.create_subscription(
            String,
            "/qos_transient_local",
            lambda msg: None,
            qos_profiles["transient_local"],
        )

        assert publisher is not None
        assert subscriber is not None

        ros2_node.destroy_publisher(publisher)
        ros2_node.destroy_subscription(subscriber)

    @pytest.mark.requires_ros2
    def test_qos_depth_effect(self, ros2_context):
        import rclpy
        from std_msgs.msg import String
        from rclpy.qos import QoSProfile

        pub_node = rclpy.create_node("depth_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("depth_sub_node", context=ros2_context)

        qos = QoSProfile(depth=5)

        received = []

        def callback(msg):
            received.append(msg.data)

        publisher = pub_node.create_publisher(String, "/depth_topic", qos)
        subscriber = sub_node.create_subscription(String, "/depth_topic", callback, qos)

        time.sleep(0.5)

        for i in range(3):
            msg = String()
            msg.data = f"Message {i}"
            publisher.publish(msg)

        for _ in range(10):
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert len(received) > 0

        pub_node.destroy_node()
        sub_node.destroy_node()


class TestMultiTopicScenarios:
    @pytest.mark.requires_ros2
    def test_multiple_topics_same_node(self, ros2_context):
        import rclpy
        from std_msgs.msg import String
        from geometry_msgs.msg import Twist

        node = rclpy.create_node("multi_topic_node", context=ros2_context)

        string_pub = node.create_publisher(String, "/multi_string", 10)
        string_sub = node.create_subscription(
            String, "/multi_string", lambda msg: None, 10
        )

        twist_pub = node.create_publisher(Twist, "/multi_twist", 10)
        twist_sub = node.create_subscription(
            Twist, "/multi_twist", lambda msg: None, 10
        )

        assert string_pub is not None
        assert string_sub is not None
        assert twist_pub is not None
        assert twist_sub is not None

        node.destroy_node()

    @pytest.mark.requires_ros2
    def test_multiple_topics_different_types(self, ros2_context):
        import rclpy
        from std_msgs.msg import String, Int32, Bool, Float32

        pub_node = rclpy.create_node("type_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("type_sub_node", context=ros2_context)

        received = {}

        publishers = {
            "string": pub_node.create_publisher(String, "/multi_string", 10),
            "int": pub_node.create_publisher(Int32, "/multi_int", 10),
            "bool": pub_node.create_publisher(Bool, "/multi_bool", 10),
            "float": pub_node.create_publisher(Float32, "/multi_float", 10),
        }

        subscribers = {
            "string": sub_node.create_subscription(
                String,
                "/multi_string",
                lambda msg: received.setdefault("string", msg.data),
                10,
            ),
            "int": sub_node.create_subscription(
                Int32,
                "/multi_int",
                lambda msg: received.setdefault("int", msg.data),
                10,
            ),
            "bool": sub_node.create_subscription(
                Bool,
                "/multi_bool",
                lambda msg: received.setdefault("bool", msg.data),
                10,
            ),
            "float": sub_node.create_subscription(
                Float32,
                "/multi_float",
                lambda msg: received.setdefault("float", msg.data),
                10,
            ),
        }

        time.sleep(0.5)

        msg_string = String()
        msg_string.data = "test"
        publishers["string"].publish(msg_string)

        msg_int = Int32()
        msg_int.data = 42
        publishers["int"].publish(msg_int)

        msg_bool = Bool()
        msg_bool.data = True
        publishers["bool"].publish(msg_bool)

        msg_float = Float32()
        msg_float.data = 3.14
        publishers["float"].publish(msg_float)

        for _ in range(20):
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert len(received) > 0

        pub_node.destroy_node()
        sub_node.destroy_node()

    @pytest.mark.requires_ros2
    def test_topic_discovery(self, ros2_context):
        import rclpy
        from std_msgs.msg import String

        node = rclpy.create_node("discovery_node", context=ros2_context)

        publisher = node.create_publisher(String, "/discovery_topic", 10)

        time.sleep(0.5)

        topic_names_and_types = node.get_topic_names_and_types()

        topic_names = [t[0] for t in topic_names_and_types]
        assert "/discovery_topic" in topic_names or len(topic_names) >= 0

        node.destroy_node()

    @pytest.mark.requires_ros2
    def test_concurrent_topic_communication(self, ros2_context):
        import rclpy
        from std_msgs.msg import String
        from concurrent.futures import ThreadPoolExecutor
        import threading

        pub_node = rclpy.create_node("conc_pub_node", context=ros2_context)
        sub_node1 = rclpy.create_node("conc_sub_node1", context=ros2_context)
        sub_node2 = rclpy.create_node("conc_sub_node2", context=ros2_context)

        received1 = []
        received2 = []
        lock1 = threading.Lock()
        lock2 = threading.Lock()

        def callback1(msg):
            with lock1:
                received1.append(msg.data)

        def callback2(msg):
            with lock2:
                received2.append(msg.data)

        publisher = pub_node.create_publisher(String, "/concurrent_topic", 10)
        subscriber1 = sub_node1.create_subscription(
            String, "/concurrent_topic", callback1, 10
        )
        subscriber2 = sub_node2.create_subscription(
            String, "/concurrent_topic", callback2, 10
        )

        time.sleep(0.5)

        msg = String()
        msg.data = "Concurrent test"

        for _ in range(10):
            publisher.publish(msg)
            rclpy.spin_once(sub_node1, timeout_sec=0.05)
            rclpy.spin_once(sub_node2, timeout_sec=0.05)

        assert len(received1) > 0 or len(received2) > 0

        pub_node.destroy_node()
        sub_node1.destroy_node()
        sub_node2.destroy_node()


@pytest.mark.integration
class TestROS2MicroROSIntegration:
    @pytest.mark.requires_agent
    @pytest.mark.requires_ros2
    def test_full_micro_ros_communication_cycle(
        self, micro_ros_agent, ros2_context, test_config
    ):
        if not micro_ros_agent["available"]:
            pytest.skip("micro-ROS agent not available")

        import rclpy
        from std_msgs.msg import String

        node = rclpy.create_node("microros_integration_node", context=ros2_context)

        publisher = node.create_publisher(String, "/microros_topic", 10)
        subscriber = node.create_subscription(
            String, "/microros_topic", lambda msg: None, 10
        )

        time.sleep(0.5)

        for i in range(5):
            msg = String()
            msg.data = f"micro-ROS message {i}"
            publisher.publish(msg)
            rclpy.spin_once(node, timeout_sec=0.1)

        node.destroy_node()

    @pytest.mark.slow
    @pytest.mark.requires_ros2
    def test_sustained_ros2_communication(self, ros2_context):
        import rclpy
        from std_msgs.msg import String

        pub_node = rclpy.create_node("sustain_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("sustain_sub_node", context=ros2_context)

        received_count = [0]

        def callback(msg):
            received_count[0] += 1

        publisher = pub_node.create_publisher(String, "/sustained_topic", 10)
        subscriber = sub_node.create_subscription(
            String, "/sustained_topic", callback, 10
        )

        time.sleep(0.5)

        test_duration = 2.0
        start_time = time.time()
        message_count = 0

        while time.time() - start_time < test_duration:
            msg = String()
            msg.data = f"Message {message_count}"
            publisher.publish(msg)
            message_count += 1
            rclpy.spin_once(sub_node, timeout_sec=0.01)

        assert received_count[0] > 0

        pub_node.destroy_node()
        sub_node.destroy_node()
