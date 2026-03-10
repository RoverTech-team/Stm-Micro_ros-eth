import pytest
import socket
import asyncio
import struct
from concurrent.futures import ThreadPoolExecutor


class TestUDPSocketCreation:
    def test_socket_creation_ipv4(self, udp_client):
        sock = udp_client()
        assert sock is not None
        assert sock.family == socket.AF_INET
        assert sock.type == socket.SOCK_DGRAM
        sock.close()

    def test_socket_creation_ipv6(self, test_config):
        try:
            sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
            assert sock is not None
            assert sock.family == socket.AF_INET6
            sock.close()
        except OSError:
            pytest.skip("IPv6 not available on this system")

    def test_socket_options(self, udp_client):
        sock = udp_client()

        reuse_addr = sock.getsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR)
        assert reuse_addr == 1

        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
        rcvbuf = sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
        assert rcvbuf >= 65536

        sock.close()


class TestBindAndConnect:
    def test_bind_to_any_port(self, udp_client):
        sock = udp_client()
        sock.bind(("0.0.0.0", 0))
        addr, port = sock.getsockname()
        assert addr == "0.0.0.0"
        assert port > 0
        sock.close()

    def test_bind_to_specific_port(self, udp_client):
        sock = udp_client()
        sock.bind(("127.0.0.1", 0))
        addr, port = sock.getsockname()
        assert addr == "127.0.0.1"
        assert port > 0
        sock.close()

    def test_bind_port_already_in_use(self, udp_client):
        sock1 = udp_client()
        sock1.bind(("127.0.0.1", 0))
        _, port = sock1.getsockname()

        sock2 = udp_client()
        with pytest.raises(OSError):
            sock2.bind(("127.0.0.1", port))

        sock1.close()
        sock2.close()

    def test_connect_to_remote(self, udp_client, test_config):
        sock = udp_client()
        sock.connect((test_config.agent_ip, test_config.agent_port))

        remote_addr = sock.getpeername()
        assert remote_addr[0] == test_config.agent_ip
        assert remote_addr[1] == test_config.agent_port
        sock.close()

    def test_connect_invalid_address(self, udp_client):
        sock = udp_client()
        with pytest.raises((socket.gaierror, OSError)):
            sock.connect(("invalid.nonexistent.domain", 9999))
        sock.close()


class TestSendReceive:
    def test_send_receive_localhost(self, udp_client):
        server_sock = udp_client()
        server_sock.bind(("127.0.0.1", 0))
        _, port = server_sock.getsockname()

        client_sock = udp_client()

        test_data = b"Hello, micro-ROS!"
        client_sock.sendto(test_data, ("127.0.0.1", port))

        received_data, addr = server_sock.recvfrom(1024)
        assert received_data == test_data
        assert addr[0] == "127.0.0.1"

        server_sock.close()
        client_sock.close()

    @pytest.mark.parametrize("data_size", [1, 64, 512, 1024, 1472])
    def test_send_various_sizes(self, udp_client, data_size):
        server_sock = udp_client()
        server_sock.bind(("127.0.0.1", 0))
        _, port = server_sock.getsockname()

        client_sock = udp_client()

        test_data = bytes(range(256)) * (data_size // 256 + 1)
        test_data = test_data[:data_size]

        client_sock.sendto(test_data, ("127.0.0.1", port))

        received_data, _ = server_sock.recvfrom(2048)
        assert received_data == test_data

        server_sock.close()
        client_sock.close()

    def test_send_empty_packet(self, udp_client):
        server_sock = udp_client()
        server_sock.bind(("127.0.0.1", 0))
        _, port = server_sock.getsockname()

        client_sock = udp_client()
        client_sock.sendto(b"", ("127.0.0.1", port))

        received_data, _ = server_sock.recvfrom(1024)
        assert received_data == b""

        server_sock.close()
        client_sock.close()


class TestTimeoutHandling:
    def test_receive_timeout(self, udp_client, test_config):
        sock = udp_client()
        sock.bind(("127.0.0.1", 0))
        sock.settimeout(0.1)

        with pytest.raises(socket.timeout):
            sock.recvfrom(1024)
        sock.close()

    def test_timeout_property(self, udp_client):
        sock = udp_client()
        sock.settimeout(5.0)
        assert sock.gettimeout() == 5.0
        sock.close()

    def test_non_blocking_mode(self, udp_client):
        sock = udp_client()
        sock.setblocking(False)

        with pytest.raises(BlockingIOError):
            sock.recvfrom(1024)
        sock.close()

    def test_blocking_mode_default(self, udp_client):
        sock = udp_client()
        assert sock.gettimeout() is None
        sock.close()


class TestErrorConditions:
    def test_connection_refused(self, udp_client):
        sock = udp_client()
        sock.settimeout(1.0)

        sock.connect(("127.0.0.1", 1))

        with pytest.raises((ConnectionRefusedError, OSError)):
            sock.send(b"test")
            sock.recv(1024)
        sock.close()

    def test_send_to_unreachable_host(self, udp_client, test_config):
        sock = udp_client()
        sock.settimeout(1.0)

        try:
            sock.sendto(b"test", ("192.0.2.1", test_config.agent_port))
        except Exception as e:
            assert isinstance(e, (socket.timeout, OSError))
        sock.close()

    def test_send_after_close(self, udp_client):
        sock = udp_client()
        sock.bind(("127.0.0.1", 0))
        sock.close()

        with pytest.raises(OSError):
            sock.sendto(b"test", ("127.0.0.1", 9999))

    def test_recv_after_close(self, udp_client):
        sock = udp_client()
        sock.bind(("127.0.0.1", 0))
        sock.close()

        with pytest.raises(OSError):
            sock.recvfrom(1024)

    def test_address_family_mismatch(self):
        ipv4_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        with pytest.raises(OSError):
            ipv4_sock.sendto(b"test", ("::1", 9999))
        ipv4_sock.close()


class TestLargePacketHandling:
    @pytest.mark.parametrize("size", [4096, 8192, 16384, 32768])
    def test_large_packet_fragmentation(self, udp_client, size):
        server_sock = udp_client()
        server_sock.bind(("127.0.0.1", 0))
        server_sock.settimeout(5.0)
        _, port = server_sock.getsockname()

        client_sock = udp_client()

        test_data = bytes([i % 256 for i in range(size)])

        try:
            sent = client_sock.sendto(test_data, ("127.0.0.1", port))

            received_chunks = []
            remaining = size
            while remaining > 0:
                chunk, _ = server_sock.recvfrom(min(65535, remaining + 1024))
                received_chunks.append(chunk)
                remaining -= len(chunk)

            received_data = b"".join(received_chunks)
            assert len(received_data) == size
            assert received_data == test_data
        except OSError:
            pytest.skip(f"Packet size {size} exceeds MTU or system limits")
        finally:
            server_sock.close()
            client_sock.close()

    def test_max_udp_packet_size(self, udp_client, test_config):
        server_sock = udp_client()
        server_sock.bind(("127.0.0.1", 0))
        _, port = server_sock.getsockname()

        client_sock = udp_client()

        max_safe_size = test_config.large_message_size
        test_data = bytes([i % 256 for i in range(max_safe_size)])

        try:
            client_sock.sendto(test_data, ("127.0.0.1", port))
        except OSError as e:
            pytest.skip(f"Large packet send failed: {e}")
        finally:
            server_sock.close()
            client_sock.close()


class TestConcurrentOperations:
    def test_concurrent_sends(self, udp_client):
        server_sock = udp_client()
        server_sock.bind(("127.0.0.1", 0))
        server_sock.settimeout(5.0)
        _, port = server_sock.getsockname()

        def send_data(client_id):
            sock = udp_client()
            for i in range(5):
                data = f"Client {client_id} message {i}".encode()
                sock.sendto(data, ("127.0.0.1", port))
            sock.close()
            return client_id

        with ThreadPoolExecutor(max_workers=4) as executor:
            futures = [executor.submit(send_data, i) for i in range(4)]
            results = [f.result() for f in futures]

        assert len(results) == 4

        received_count = 0
        try:
            server_sock.settimeout(0.5)
            while True:
                server_sock.recvfrom(1024)
                received_count += 1
        except socket.timeout:
            pass

        server_sock.close()

    def test_concurrent_send_receive(self, udp_client):
        server_sock = udp_client()
        server_sock.bind(("127.0.0.1", 0))
        _, port = server_sock.getsockname()

        client1 = udp_client()
        client2 = udp_client()

        client1.sendto(b"from client1", ("127.0.0.1", port))
        client2.sendto(b"from client2", ("127.0.0.1", port))

        server_sock.settimeout(2.0)
        data1, addr1 = server_sock.recvfrom(1024)
        data2, addr2 = server_sock.recvfrom(1024)

        assert data1 in [b"from client1", b"from client2"]
        assert data2 in [b"from client1", b"from client2"]

        server_sock.close()
        client1.close()
        client2.close()


class TestUDPMicroROSProtocol:
    def test_microros_transport_header(self, udp_client):
        server_sock = udp_client()
        server_sock.bind(("127.0.0.1", 0))
        _, port = server_sock.getsockname()

        client_sock = udp_client()

        header = struct.pack("<BBBB", 0x00, 0x00, 0x00, 0x00)
        payload = b"micro-ROS test payload"
        message = header + payload

        client_sock.sendto(message, ("127.0.0.1", port))

        server_sock.settimeout(2.0)
        received, addr = server_sock.recvfrom(1024)

        assert received[:4] == header
        assert received[4:] == payload

        server_sock.close()
        client_sock.close()

    def test_microros_session_id(self, udp_client):
        server_sock = udp_client()
        server_sock.bind(("127.0.0.1", 0))
        _, port = server_sock.getsockname()

        client_sock = udp_client()

        for session_id in [0x01, 0x7F, 0x80, 0xFF]:
            header = struct.pack("<B", session_id)
            client_sock.sendto(header + b"data", ("127.0.0.1", port))

            server_sock.settimeout(1.0)
            received, _ = server_sock.recvfrom(1024)
            assert received[0] == session_id

        server_sock.close()
        client_sock.close()


@pytest.mark.integration
class TestUDPTransportIntegration:
    def test_bidirectional_communication(self, udp_client):
        server_sock = udp_client()
        server_sock.bind(("127.0.0.1", 0))
        _, port = server_sock.getsockname()

        client_sock = udp_client()
        client_sock.connect(("127.0.0.1", port))

        request = b"REQUEST"
        client_sock.send(request)

        server_sock.settimeout(2.0)
        received, addr = server_sock.recvfrom(1024)
        assert received == request

        response = b"RESPONSE"
        server_sock.sendto(response, addr)

        client_response = client_sock.recv(1024)
        assert client_response == response

        server_sock.close()
        client_sock.close()

    @pytest.mark.requires_agent
    def test_communication_with_agent(self, udp_client, micro_ros_agent, test_config):
        if not micro_ros_agent["available"]:
            pytest.skip("micro-ROS agent not available")

        sock = udp_client()
        sock.settimeout(5.0)

        try:
            discovery_message = bytes([0x00] * 8)
            sock.sendto(
                discovery_message, (test_config.agent_ip, test_config.agent_port)
            )

            response, _ = sock.recvfrom(1024)
            assert len(response) > 0
        except socket.timeout:
            pytest.skip("No response from micro-ROS agent")
        finally:
            sock.close()
