import pytest
import socket
import time
import struct


class TestAgentDiscovery:
    @pytest.mark.requires_agent
    def test_agent_discovery_packet(self, micro_ros_agent, test_config):
        if not micro_ros_agent["available"]:
            pytest.skip("micro-ROS agent not available")

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(test_config.discovery_timeout_ms / 1000.0)

        try:
            discovery = bytes([0x00, 0x00, 0x00, 0x00])
            sock.sendto(discovery, (test_config.agent_ip, test_config.agent_port))

            response, addr = sock.recvfrom(1024)
            assert addr[0] == test_config.agent_ip
            assert addr[1] == test_config.agent_port
            assert len(response) > 0
        except socket.timeout:
            pytest.skip("Agent discovery timed out")
        finally:
            sock.close()

    @pytest.mark.requires_agent
    def test_agent_multiple_discovery_attempts(self, micro_ros_agent, test_config):
        if not micro_ros_agent["available"]:
            pytest.skip("micro-ROS agent not available")

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(test_config.discovery_timeout_ms / 1000.0)

        successful_discoveries = 0
        max_attempts = test_config.max_retries

        try:
            for attempt in range(max_attempts):
                discovery = bytes([0x00] * 4)
                sock.sendto(discovery, (test_config.agent_ip, test_config.agent_port))

                try:
                    response, _ = sock.recvfrom(1024)
                    if len(response) > 0:
                        successful_discoveries += 1
                except socket.timeout:
                    pass

                time.sleep(test_config.retry_delay_ms / 1000.0)

            assert successful_discoveries >= 1
        finally:
            sock.close()

    def test_agent_not_available_handling(self, test_config):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(1.0)

        try:
            discovery = bytes([0x00] * 4)
            sock.sendto(discovery, ("127.0.0.1", 1))

            with pytest.raises((socket.timeout, ConnectionRefusedError, OSError)):
                sock.recvfrom(1024)
        finally:
            sock.close()


class TestParticipantCreation:
    @pytest.mark.requires_agent
    @pytest.mark.requires_ros2
    def test_participant_creation_via_agent(self, micro_ros_agent, dds_participant):
        if not micro_ros_agent["available"]:
            pytest.skip("micro-ROS agent not available")

        assert dds_participant is not None
        assert dds_participant.get_name() == "test_dds_participant"

    @pytest.mark.requires_ros2
    def test_participant_domain_id(self, ros2_context):
        import rclpy
        from rclpy.node import Node

        node = rclpy.create_node("domain_test_node", context=ros2_context)

        assert node is not None
        node.destroy_node()

    @pytest.mark.requires_ros2
    def test_multiple_participants(self, ros2_context):
        import rclpy
        from rclpy.node import Node

        participants = []
        for i in range(3):
            node = rclpy.create_node(f"participant_{i}", context=ros2_context)
            participants.append(node)

        assert len(participants) == 3

        for node in participants:
            node.destroy_node()


class TestPublisherSubscriberCreation:
    @pytest.mark.requires_ros2
    def test_publisher_creation(self, ros2_node, message_factory):
        from std_msgs.msg import String

        publisher = ros2_node.create_publisher(String, "/test_topic", 10)

        assert publisher is not None
        assert publisher.topic_name == "/test_topic"

        ros2_node.destroy_publisher(publisher)

    @pytest.mark.requires_ros2
    def test_subscriber_creation(self, ros2_node, message_factory):
        from std_msgs.msg import String

        subscriber = ros2_node.create_subscription(
            String, "/test_topic", lambda msg: None, 10
        )

        assert subscriber is not None

        ros2_node.destroy_subscription(subscriber)

    @pytest.mark.requires_ros2
    def test_publisher_subscriber_pair(self, ros2_node):
        from std_msgs.msg import String

        publisher = ros2_node.create_publisher(String, "/pair_test", 10)
        subscriber = ros2_node.create_subscription(
            String, "/pair_test", lambda msg: None, 10
        )

        assert publisher is not None
        assert subscriber is not None

        ros2_node.destroy_publisher(publisher)
        ros2_node.destroy_subscription(subscriber)

    @pytest.mark.requires_ros2
    @pytest.mark.parametrize("qos_type", ["reliable", "best_effort", "transient_local"])
    def test_publisher_with_qos(self, ros2_node, qos_profiles, qos_type):
        from std_msgs.msg import String

        publisher = ros2_node.create_publisher(
            String, f"/qos_test_{qos_type}", qos_profiles[qos_type]
        )

        assert publisher is not None

        ros2_node.destroy_publisher(publisher)


class TestTopicCommunication:
    @pytest.mark.requires_ros2
    def test_simple_message_exchange(self, ros2_context):
        import rclpy
        from std_msgs.msg import String

        pub_node = rclpy.create_node("publisher_node", context=ros2_context)
        sub_node = rclpy.create_node("subscriber_node", context=ros2_context)

        received_messages = []

        def callback(msg):
            received_messages.append(msg.data)

        publisher = pub_node.create_publisher(String, "/simple_test", 10)
        subscriber = sub_node.create_subscription(String, "/simple_test", callback, 10)

        test_message = "Hello micro-ROS!"
        msg = String()
        msg.data = test_message

        time.sleep(0.5)

        for _ in range(10):
            publisher.publish(msg)
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert len(received_messages) > 0
        assert test_message in received_messages

        pub_node.destroy_node()
        sub_node.destroy_node()

    @pytest.mark.requires_ros2
    def test_multiple_messages_sequence(self, ros2_context):
        import rclpy
        from std_msgs.msg import String

        pub_node = rclpy.create_node("seq_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("seq_sub_node", context=ros2_context)

        received = []

        def callback(msg):
            received.append(msg.data)

        publisher = pub_node.create_publisher(String, "/sequence_test", 10)
        subscriber = sub_node.create_subscription(
            String, "/sequence_test", callback, 10
        )

        time.sleep(0.5)

        for i in range(10):
            msg = String()
            msg.data = f"Message_{i}"
            publisher.publish(msg)
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        for i in range(5):
            rclpy.spin_once(sub_node, timeout_sec=0.1)

        assert len(received) >= 1

        pub_node.destroy_node()
        sub_node.destroy_node()


class TestAgentReconnection:
    @pytest.mark.requires_agent
    def test_reconnection_after_timeout(self, micro_ros_agent, test_config):
        if not micro_ros_agent["available"]:
            pytest.skip("micro-ROS agent not available")

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(2.0)

        try:
            discovery = bytes([0x00] * 4)
            sock.sendto(discovery, (test_config.agent_ip, test_config.agent_port))
            response1, _ = sock.recvfrom(1024)
            assert len(response1) > 0

            sock.close()
            time.sleep(0.5)

            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.settimeout(2.0)

            sock.sendto(discovery, (test_config.agent_ip, test_config.agent_port))
            response2, _ = sock.recvfrom(1024)
            assert len(response2) > 0
        except socket.timeout:
            pytest.skip("Agent reconnection timed out")
        finally:
            sock.close()

    @pytest.mark.requires_agent
    def test_multiple_client_connections(self, micro_ros_agent, test_config):
        if not micro_ros_agent["available"]:
            pytest.skip("micro-ROS agent not available")

        clients = []

        for i in range(3):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.settimeout(5.0)
            clients.append(sock)

        try:
            for i, sock in enumerate(clients):
                discovery = bytes([0x00] * 4)
                sock.sendto(discovery, (test_config.agent_ip, test_config.agent_port))

            responses = 0
            for sock in clients:
                try:
                    _, _ = sock.recvfrom(1024)
                    responses += 1
                except socket.timeout:
                    pass

            assert responses >= 1
        finally:
            for sock in clients:
                sock.close()


class TestErrorHandling:
    @pytest.mark.requires_ros2
    def test_invalid_topic_name(self, ros2_node):
        from std_msgs.msg import String

        with pytest.raises(Exception):
            ros2_node.create_publisher(String, "", 10)

    @pytest.mark.requires_ros2
    def test_invalid_qos_depth(self, ros2_node):
        from std_msgs.msg import String
        from rclpy.qos import QoSProfile

        qos = QoSProfile(depth=0)

        publisher = ros2_node.create_publisher(String, "/invalid_qos_test", qos)

        if publisher:
            ros2_node.destroy_publisher(publisher)

    def test_agent_unavailable_graceful_handling(self, test_config):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(0.5)

        try:
            sock.sendto(b"test", ("127.0.0.1", 1))
            sock.recvfrom(1024)
            assert False, "Should have timed out"
        except socket.timeout:
            assert True
        finally:
            sock.close()

    @pytest.mark.requires_ros2
    def test_subscriber_callback_exception_handling(self, ros2_context):
        import rclpy
        from std_msgs.msg import String

        pub_node = rclpy.create_node("exc_pub_node", context=ros2_context)
        sub_node = rclpy.create_node("exc_sub_node", context=ros2_context)

        callback_invoked = [False]

        def failing_callback(msg):
            callback_invoked[0] = True
            raise Exception("Callback error")

        publisher = pub_node.create_publisher(String, "/exception_test", 10)
        subscriber = sub_node.create_subscription(
            String, "/exception_test", failing_callback, 10
        )

        time.sleep(0.5)

        msg = String()
        msg.data = "test"
        publisher.publish(msg)

        rclpy.spin_once(sub_node, timeout_sec=0.5)

        assert callback_invoked[0]

        pub_node.destroy_node()
        sub_node.destroy_node()


@pytest.mark.integration
class TestAgentIntegration:
    @pytest.mark.requires_agent
    @pytest.mark.requires_ros2
    def test_full_communication_cycle(self, micro_ros_agent, ros2_context, test_config):
        if not micro_ros_agent["available"]:
            pytest.skip("micro-ROS agent not available")

        import rclpy
        from std_msgs.msg import String

        node = rclpy.create_node("integration_test_node", context=ros2_context)

        received = []

        def callback(msg):
            received.append(msg.data)

        publisher = node.create_publisher(String, "/integration_topic", 10)
        subscriber = node.create_subscription(
            String, "/integration_topic", callback, 10
        )

        time.sleep(0.5)

        for i in range(5):
            msg = String()
            msg.data = f"Integration test message {i}"
            publisher.publish(msg)
            rclpy.spin_once(node, timeout_sec=0.1)

        for _ in range(10):
            rclpy.spin_once(node, timeout_sec=0.1)

        assert len(received) >= 1

        node.destroy_node()

    @pytest.mark.slow
    @pytest.mark.requires_agent
    def test_sustained_communication(self, micro_ros_agent, test_config):
        if not micro_ros_agent["available"]:
            pytest.skip("micro-ROS agent not available")

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(5.0)

        successful_exchanges = 0
        test_duration_sec = 5
        start_time = time.time()

        try:
            while time.time() - start_time < test_duration_sec:
                discovery = bytes([0x00] * 4)
                sock.sendto(discovery, (test_config.agent_ip, test_config.agent_port))

                try:
                    _, _ = sock.recvfrom(1024)
                    successful_exchanges += 1
                except socket.timeout:
                    pass

                time.sleep(0.1)

            assert successful_exchanges > 0
        finally:
            sock.close()
