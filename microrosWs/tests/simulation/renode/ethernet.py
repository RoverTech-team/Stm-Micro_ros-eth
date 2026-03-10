#!/usr/bin/env python3
"""
Ethernet Emulation Python Hooks for Renode STM32H7 Simulation

This module provides Python hooks for Ethernet emulation in Renode,
enabling advanced packet manipulation, monitoring, and error injection
capabilities for micro-ROS testing.

Features:
- Virtual Ethernet interface creation
- Packet injection for testing
- Network statistics collection
- Error injection for fault tolerance testing
- XRCE-DDS packet analysis

Usage in Renode:
    emulation LoadPythonExtension "ethernet.py"
    eth = EthernetHook.Create("eth_mac")
    eth.InjectPacket("test.bin")
    eth.EnableStatistics(True)
"""

import struct
import time
import threading
import socket
import select
from collections import deque, Counter
from dataclasses import dataclass, field
from typing import Optional, Callable, List, Tuple, Dict, Any
from enum import IntEnum
import json
import logging
import os

logger = logging.getLogger(__name__)


# ========================================
# Ethernet Constants
# ========================================

ETH_HEADER_SIZE = 14
ETH_MIN_FRAME_SIZE = 60
ETH_MAX_FRAME_SIZE = 1518
ETH_JUMBO_MAX = 9000


# EtherType values
class EtherType(IntEnum):
    IPV4 = 0x0800
    ARP = 0x0806
    IPV6 = 0x86DD
    VLAN = 0x8100
    LLDP = 0x88CC
    PTP = 0x88F7


# ========================================
# UDP/XRCE-DDS Constants
# ========================================

MICRO_ROS_DEFAULT_PORT = 8888
XRCE_DDS_MAGIC = 0x58524345  # "XRCE"


class XRCEMessageKind(IntEnum):
    DATA = 0x01
    HEARTBEAT = 0x04
    ACKNACK = 0x05
    ACKNACK_FRAG = 0x06
    HEARTBEAT_FRAG = 0x07
    DATA_FRAG = 0x08
    CREATE_CLIENT = 0x20
    CREATE = 0x01
    GET_INFO = 0x03
    DELETE = 0x04
    WRITE_DATA = 0x05
    READ_DATA = 0x06
    ACKREAD = 0x07
    NACKREAD = 0x08


# ========================================
# Data Classes
# ========================================


@dataclass
class EthernetFrame:
    """Represents an Ethernet frame with parsed headers."""

    dst_mac: bytes
    src_mac: bytes
    ethertype: int
    payload: bytes
    vlan_tag: Optional[int] = None

    @classmethod
    def from_bytes(cls, data: bytes) -> "EthernetFrame":
        """Parse raw bytes into EthernetFrame."""
        if len(data) < ETH_HEADER_SIZE:
            raise ValueError(f"Frame too short: {len(data)} bytes")

        dst_mac = data[0:6]
        src_mac = data[6:12]

        offset = 12
        ethertype = struct.unpack("!H", data[12:14])[0]
        vlan_tag = None

        # Handle VLAN tag
        if ethertype == EtherType.VLAN:
            vlan_tag = struct.unpack("!H", data[14:16])[0]
            offset = 16
            ethertype = struct.unpack("!H", data[16:18])[0]
            offset = 18

        payload = data[offset:]

        return cls(
            dst_mac=dst_mac,
            src_mac=src_mac,
            ethertype=ethertype,
            payload=payload,
            vlan_tag=vlan_tag,
        )

    def to_bytes(self) -> bytes:
        """Convert EthernetFrame to raw bytes."""
        result = bytearray()
        result.extend(self.dst_mac)
        result.extend(self.src_mac)

        if self.vlan_tag is not None:
            result.extend(struct.pack("!H", EtherType.VLAN))
            result.extend(struct.pack("!H", self.vlan_tag))

        result.extend(struct.pack("!H", self.ethertype))
        result.extend(self.payload)

        return bytes(result)

    def is_broadcast(self) -> bool:
        """Check if destination is broadcast."""
        return all(b == 0xFF for b in self.dst_mac)

    def is_multicast(self) -> bool:
        """Check if destination is multicast."""
        return (self.dst_mac[0] & 0x01) != 0

    def __str__(self) -> str:
        dst = ":".join(f"{b:02X}" for b in self.dst_mac)
        src = ":".join(f"{b:02X}" for b in self.src_mac)
        return f"EthernetFrame[{src} -> {dst}, type=0x{self.ethertype:04X}, len={len(self.payload)}]"


@dataclass
class IPHeader:
    """IPv4 header representation."""

    version: int
    ihl: int
    tos: int
    total_length: int
    identification: int
    flags: int
    fragment_offset: int
    ttl: int
    protocol: int
    checksum: int
    src_addr: bytes
    dst_addr: bytes

    @classmethod
    def from_bytes(cls, data: bytes) -> "IPHeader":
        """Parse IPv4 header from bytes."""
        if len(data) < 20:
            raise ValueError("IP header too short")

        version_ihl = data[0]
        version = (version_ihl >> 4) & 0x0F
        ihl = version_ihl & 0x0F

        tos = data[1]
        total_length = struct.unpack("!H", data[2:4])[0]
        identification = struct.unpack("!H", data[4:6])[0]

        flags_frag = struct.unpack("!H", data[6:8])[0]
        flags = (flags_frag >> 13) & 0x07
        fragment_offset = flags_frag & 0x1FFF

        ttl = data[8]
        protocol = data[9]
        checksum = struct.unpack("!H", data[10:12])[0]

        src_addr = data[12:16]
        dst_addr = data[16:20]

        return cls(
            version=version,
            ihl=ihl,
            tos=tos,
            total_length=total_length,
            identification=identification,
            flags=flags,
            fragment_offset=fragment_offset,
            ttl=ttl,
            protocol=protocol,
            checksum=checksum,
            src_addr=src_addr,
            dst_addr=dst_addr,
        )

    def src_ip_str(self) -> str:
        return ".".join(str(b) for b in self.src_addr)

    def dst_ip_str(self) -> str:
        return ".".join(str(b) for b in self.dst_addr)


@dataclass
class UDPHeader:
    """UDP header representation."""

    src_port: int
    dst_port: int
    length: int
    checksum: int

    @classmethod
    def from_bytes(cls, data: bytes) -> "UDPHeader":
        """Parse UDP header from bytes."""
        if len(data) < 8:
            raise ValueError("UDP header too short")

        src_port = struct.unpack("!H", data[0:2])[0]
        dst_port = struct.unpack("!H", data[2:4])[0]
        length = struct.unpack("!H", data[4:6])[0]
        checksum = struct.unpack("!H", data[6:8])[0]

        return cls(
            src_port=src_port, dst_port=dst_port, length=length, checksum=checksum
        )


@dataclass
class NetworkStatistics:
    """Network statistics collection."""

    tx_packets: int = 0
    tx_bytes: int = 0
    tx_errors: int = 0
    tx_dropped: int = 0

    rx_packets: int = 0
    rx_bytes: int = 0
    rx_errors: int = 0
    rx_dropped: int = 0

    crc_errors: int = 0
    frame_errors: int = 0
    collisions: int = 0
    overruns: int = 0

    start_time: float = field(default_factory=time.time)

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        elapsed = time.time() - self.start_time
        return {
            "tx": {
                "packets": self.tx_packets,
                "bytes": self.tx_bytes,
                "errors": self.tx_errors,
                "dropped": self.tx_dropped,
                "rate_pps": self.tx_packets / elapsed if elapsed > 0 else 0,
                "rate_bps": self.tx_bytes / elapsed if elapsed > 0 else 0,
            },
            "rx": {
                "packets": self.rx_packets,
                "bytes": self.rx_bytes,
                "errors": self.rx_errors,
                "dropped": self.rx_dropped,
                "rate_pps": self.rx_packets / elapsed if elapsed > 0 else 0,
                "rate_bps": self.rx_bytes / elapsed if elapsed > 0 else 0,
            },
            "errors": {
                "crc": self.crc_errors,
                "frame": self.frame_errors,
                "collisions": self.collisions,
                "overruns": self.overruns,
            },
            "elapsed_seconds": elapsed,
        }

    def reset(self):
        """Reset all counters."""
        self.tx_packets = 0
        self.tx_bytes = 0
        self.tx_errors = 0
        self.tx_dropped = 0
        self.rx_packets = 0
        self.rx_bytes = 0
        self.rx_errors = 0
        self.rx_dropped = 0
        self.crc_errors = 0
        self.frame_errors = 0
        self.collisions = 0
        self.overruns = 0
        self.start_time = time.time()


# ========================================
# XRCE-DDS Analyzer
# ========================================


class XRCEAnalyzer:
    """Analyzes XRCE-DDS protocol messages."""

    def __init__(self):
        self.messages: List[Dict] = []
        self.client_sessions: Dict[int, Dict] = {}

    def analyze_packet(self, payload: bytes) -> Optional[Dict]:
        """Analyze XRCE-DDS message payload."""
        if len(payload) < 4:
            logger.debug("XRCE: payload too short (%d bytes), skipping", len(payload))
            return None

        # Check for XRCE magic number
        magic = struct.unpack("!I", payload[0:4])[0]
        if magic != XRCE_DDS_MAGIC:
            logger.debug(
                "XRCE: magic mismatch (got 0x%08X, expected 0x%08X), not XRCE-DDS",
                magic,
                XRCE_DDS_MAGIC,
            )
            return None

        logger.debug(
            "XRCE: magic detected (0x%08X), payload length=%d", magic, len(payload)
        )
        result = {"magic": "XRCE", "timestamp": time.time(), "raw_length": len(payload)}

        # Parse XRCE header
        if len(payload) >= 12:
            session_id = payload[4]
            stream_id = payload[5]
            sequence = struct.unpack("!H", payload[6:8])[0]

            logger.debug(
                "XRCE: session_id=0x%02X, stream_id=0x%02X, sequence=%d",
                session_id,
                stream_id,
                sequence,
            )

            result["session_id"] = session_id
            result["stream_id"] = stream_id
            result["sequence"] = sequence

            # Track client sessions
            if session_id not in self.client_sessions:
                self.client_sessions[session_id] = {
                    "first_seen": time.time(),
                    "message_count": 0,
                    "sequences": [],
                }
                logger.debug("XRCE: new client session 0x%02X registered", session_id)

            self.client_sessions[session_id]["message_count"] += 1
            self.client_sessions[session_id]["sequences"].append(sequence)

            # Parse submessages if present
            if len(payload) > 12:
                submessages = self._parse_submessages(payload[12:])
                result["submessages"] = submessages

        self.messages.append(result)
        return result

    def _parse_submessages(self, data: bytes) -> List[Dict]:
        """Parse XRCE-DDS submessages."""
        submessages = []
        offset = 0

        while offset + 4 <= len(data):
            submessage_id = data[offset]
            flags = data[offset + 1]
            submessage_length = struct.unpack("!H", data[offset + 2 : offset + 4])[0]

            submsg_name = self._get_submessage_name(submessage_id)
            logger.debug(
                "XRCE submessage: id=0x%02X, name=%s, flags=0x%02X, length=%d",
                submessage_id,
                submsg_name,
                flags,
                submessage_length,
            )

            submsg = {
                "id": submessage_id,
                "id_name": submsg_name,
                "flags": flags,
                "length": submessage_length,
            }

            # Extract payload if present
            payload_start = offset + 4
            payload_end = payload_start + submessage_length
            if payload_end <= len(data):
                submsg["payload"] = data[payload_start:payload_end].hex()

            submessages.append(submsg)
            offset = payload_end

        logger.debug("XRCE: parsed %d submessage(s)", len(submessages))
        return submessages

    def _get_submessage_name(self, msg_id: int) -> str:
        """Get human-readable submessage name."""
        names = {
            0x01: "DATA",
            0x04: "HEARTBEAT",
            0x05: "ACKNACK",
            0x06: "ACKNACK_FRAG",
            0x07: "HEARTBEAT_FRAG",
            0x08: "DATA_FRAG",
            0x20: "CREATE_CLIENT",
            0x21: "CREATE",
            0x22: "GET_INFO",
            0x23: "DELETE",
            0x24: "WRITE_DATA",
            0x25: "READ_DATA",
            0x26: "ACKREAD",
            0x27: "NACKREAD",
        }
        return names.get(msg_id, f"UNKNOWN(0x{msg_id:02X})")

    def get_summary(self) -> Dict:
        """Get analysis summary."""
        logger.debug(
            "XRCE summary requested: total_messages=%d, client_sessions=%d",
            len(self.messages),
            len(self.client_sessions),
        )
        return {
            "total_messages": len(self.messages),
            "client_sessions": len(self.client_sessions),
            "sessions": self.client_sessions,
        }


# ========================================
# Virtual Ethernet Interface
# ========================================


class VirtualEthernetInterface:
    """
    Virtual Ethernet interface for Renode simulation.

    Provides a virtual network interface that can:
    - Connect to a TAP device for host system integration
    - Bridge multiple Renode instances
    - Connect to a UDP tunnel for distributed simulation
    """

    def __init__(self, name: str = "renode_eth"):
        self.name = name
        self.mac_address = bytes([0x00, 0x02, 0xF7, 0x00, 0x00, 0x01])

        self.rx_callback: Optional[Callable[[bytes], None]] = None
        self.statistics = NetworkStatistics()
        self.analyzer = XRCEAnalyzer()

        self.enabled = False
        self.promiscuous = False

        # Packet queues
        self.tx_queue: deque = deque(maxlen=256)
        self.rx_queue: deque = deque(maxlen=256)

        # TAP interface (optional)
        self.tap_fd: Optional[int] = None
        self.tap_name: Optional[str] = None

        # UDP tunnel (optional)
        self.udp_socket: Optional[socket.socket] = None
        self.remote_addr: Optional[Tuple[str, int]] = None

        # Thread for async processing
        self._running = False
        self._thread: Optional[threading.Thread] = None

        # Error injection flags
        self._inject_crc_error = False
        self._inject_frame_error = False
        self._inject_drop = False
        self._inject_delay_ms = 0

    def enable(self):
        """Enable the virtual interface."""
        self.enabled = True
        self._running = True
        self._thread = threading.Thread(target=self._process_loop, daemon=True)
        self._thread.start()
        logger.debug(
            "VirtualEthernetInterface '%s' enabled, MAC=%s",
            self.name,
            ":".join(f"{b:02X}" for b in self.mac_address),
        )

    def disable(self):
        """Disable the virtual interface."""
        logger.debug("VirtualEthernetInterface '%s' disabling", self.name)
        self._running = False
        self.enabled = False
        if self._thread:
            self._thread.join(timeout=1.0)
            self._thread = None
        logger.debug("VirtualEthernetInterface '%s' disabled", self.name)

    def set_mac_address(self, mac: bytes):
        """Set MAC address for this interface."""
        if len(mac) != 6:
            raise ValueError("MAC address must be 6 bytes")
        self.mac_address = mac

    def set_rx_callback(self, callback: Callable[[bytes], None]):
        """Set callback for received packets."""
        self.rx_callback = callback

    def send_packet(self, data: bytes) -> bool:
        """
        Send packet through the interface.

        Called by the peripheral when transmitting.
        """
        if not self.enabled:
            self.statistics.tx_dropped += 1
            logger.debug(
                "send_packet: interface disabled, dropping %d bytes", len(data)
            )
            return False

        self.statistics.tx_packets += 1
        self.statistics.tx_bytes += len(data)

        # Apply error injection
        if self._inject_drop:
            self._inject_drop = False
            self.statistics.tx_dropped += 1
            logger.debug("send_packet: error injection - dropping packet")
            return True

        # Parse and log
        try:
            frame = EthernetFrame.from_bytes(data)
            src_mac = ":".join(f"{b:02X}" for b in frame.src_mac)
            dst_mac = ":".join(f"{b:02X}" for b in frame.dst_mac)
            logger.debug(
                "send_packet: %s -> %s, ethertype=0x%04X, length=%d",
                src_mac,
                dst_mac,
                frame.ethertype,
                len(data),
            )

            # Analyze if XRCE-DDS
            if frame.ethertype == EtherType.IPV4:
                self._analyze_ip_packet(frame.payload)

        except Exception as e:
            logger.debug("send_packet: failed to parse frame: %s", e)

        # Queue for processing
        self.tx_queue.append(data)

        # Send to TAP if connected
        if self.tap_fd is not None:
            try:
                logger.debug(
                    "send_packet: writing %d bytes to TAP device '%s'",
                    len(data),
                    self.tap_name,
                )
                os.write(self.tap_fd, data)
            except Exception as e:
                logger.debug("send_packet: TAP write failed: %s", e)

        # Send to UDP tunnel if connected
        if self.udp_socket and self.remote_addr:
            try:
                self.udp_socket.sendto(data, self.remote_addr)
            except:
                pass

        return True

    def receive_packet(self, data: bytes):
        """
        Receive packet from external source.

        Called when packet arrives from TAP or UDP tunnel.
        """
        logger.debug("receive_packet: arrived %d bytes", len(data))
        if not self.enabled:
            self.statistics.rx_dropped += 1
            logger.debug("receive_packet: interface disabled, dropping")
            return

        # Apply error injection
        if self._inject_crc_error:
            self._inject_crc_error = False
            data = self._corrupt_crc(data)
            self.statistics.crc_errors += 1
            logger.debug("receive_packet: injected CRC error")

        if self._inject_frame_error:
            self._inject_frame_error = False
            data = self._corrupt_frame(data)
            self.statistics.frame_errors += 1
            logger.debug("receive_packet: injected frame error")

        self.statistics.rx_packets += 1
        self.statistics.rx_bytes += len(data)

        # Apply delay
        if self._inject_delay_ms > 0:
            logger.debug("receive_packet: injecting %d ms delay", self._inject_delay_ms)
            time.sleep(self._inject_delay_ms / 1000.0)
            self._inject_delay_ms = 0

        # Filter if not promiscuous
        if not self.promiscuous:
            dst_mac = data[0:6]
            if not self._is_for_us(dst_mac):
                dst_str = ":".join(f"{b:02X}" for b in dst_mac)
                our_mac = ":".join(f"{b:02X}" for b in self.mac_address)
                logger.debug(
                    "receive_packet: MAC filter reject (dst=%s, our=%s)",
                    dst_str,
                    our_mac,
                )
                self.statistics.rx_dropped += 1
                return
            else:
                dst_str = ":".join(f"{b:02X}" for b in dst_mac)
                logger.debug("receive_packet: MAC filter accepted (dst=%s)", dst_str)

        # Queue for delivery
        self.rx_queue.append(data)

        # Deliver via callback
        if self.rx_callback:
            self.rx_callback(data)

    def _is_for_us(self, dst_mac: bytes) -> bool:
        """Check if packet is addressed to us."""
        # Broadcast
        if all(b == 0xFF for b in dst_mac):
            return True

        # Unicast to us
        if dst_mac == self.mac_address:
            return True

        # Multicast
        if (dst_mac[0] & 0x01) != 0:
            return True

        return False

    def _analyze_ip_packet(self, payload: bytes):
        """Analyze IP packet for XRCE-DDS traffic."""
        try:
            ip = IPHeader.from_bytes(payload)
            logger.debug(
                "_analyze_ip_packet: %s -> %s, protocol=%d, ihl=%d",
                ip.src_ip_str(),
                ip.dst_ip_str(),
                ip.protocol,
                ip.ihl,
            )

            if ip.protocol == 17:  # UDP
                udp_payload = payload[ip.ihl * 4 :]
                udp = UDPHeader.from_bytes(udp_payload)
                logger.debug(
                    "_analyze_ip_packet: UDP ports src=%d, dst=%d, length=%d",
                    udp.src_port,
                    udp.dst_port,
                    udp.length,
                )

                # Check for micro-ROS port
                if (
                    udp.dst_port == MICRO_ROS_DEFAULT_PORT
                    or udp.src_port == MICRO_ROS_DEFAULT_PORT
                ):
                    logger.debug(
                        "_analyze_ip_packet: micro-ROS port %d detected, analyzing XRCE-DDS",
                        MICRO_ROS_DEFAULT_PORT,
                    )
                    udp_data = udp_payload[8:]
                    self.analyzer.analyze_packet(udp_data)

        except Exception as e:
            logger.debug("_analyze_ip_packet: failed to parse: %s", e)

    def _process_loop(self):
        """Background processing loop."""
        while self._running:
            # Process TAP input
            if self.tap_fd is not None:
                try:
                    ready, _, _ = select.select([self.tap_fd], [], [], 0.01)
                    if ready:
                        data = os.read(self.tap_fd, ETH_MAX_FRAME_SIZE)
                        logger.debug(
                            "_process_loop: TAP read event, %d bytes from '%s'",
                            len(data),
                            self.tap_name,
                        )
                        self.receive_packet(data)
                except Exception as e:
                    logger.debug("_process_loop: TAP read error: %s", e)

            # Process UDP input
            if self.udp_socket is not None:
                try:
                    ready, _, _ = select.select([self.udp_socket], [], [], 0.01)
                    if ready:
                        data, addr = self.udp_socket.recvfrom(ETH_MAX_FRAME_SIZE)
                        logger.debug(
                            "_process_loop: UDP read event, %d bytes from %s",
                            len(data),
                            addr,
                        )
                        self.receive_packet(data)
                except:
                    pass

            time.sleep(0.001)

    def _corrupt_crc(self, data: bytes) -> bytes:
        """Corrupt CRC for error injection."""
        if len(data) < 4:
            return data
        # Flip last byte
        result = bytearray(data)
        result[-1] ^= 0xFF
        return bytes(result)

    def _corrupt_frame(self, data: bytes) -> bytes:
        """Corrupt frame for error injection."""
        if len(data) < 10:
            return data
        # Corrupt length field
        result = bytearray(data)
        result[12] ^= 0xFF
        return bytes(result)

    # ========================================
    # TAP Interface Support
    # ========================================

    def connect_tap(self, tap_name: str = "tap0") -> bool:
        """
        Connect to a TAP device for host networking.

        Requires root privileges on Linux.
        """
        logger.debug("connect_tap: attempting to open TAP device '%s'", tap_name)
        try:
            import fcntl

            TUNSETIFF = 0x400454CA
            IFF_TAP = 0x0002
            IFF_NO_PI = 0x1000

            self.tap_fd = os.open("/dev/net/tun", os.O_RDWR | os.O_NONBLOCK)
            logger.debug("connect_tap: opened /dev/net/tun, fd=%d", self.tap_fd)

            ifr = struct.pack("16sH", tap_name.encode(), IFF_TAP | IFF_NO_PI)
            result = fcntl.ioctl(self.tap_fd, TUNSETIFF, ifr)
            logger.debug(
                "connect_tap: ioctl TUNSETIFF succeeded, result=%s",
                result.hex() if result else "None",
            )

            self.tap_name = tap_name
            logger.debug(
                "connect_tap: TAP device '%s' connected successfully (fd=%d)",
                tap_name,
                self.tap_fd,
            )
            return True

        except Exception as e:
            logger.debug("connect_tap: failed to connect TAP '%s': %s", tap_name, e)
            print(f"Failed to connect TAP: {e}")
            return False

    def disconnect_tap(self):
        """Disconnect from TAP device."""
        if self.tap_fd is not None:
            logger.debug(
                "disconnect_tap: closing TAP device '%s' (fd=%d)",
                self.tap_name,
                self.tap_fd,
            )
            os.close(self.tap_fd)
            self.tap_fd = None
            self.tap_name = None
            logger.debug("disconnect_tap: TAP device disconnected")

    # ========================================
    # UDP Tunnel Support
    # ========================================

    def connect_udp_tunnel(
        self, local_port: int, remote_host: str, remote_port: int
    ) -> bool:
        """
        Connect to a UDP tunnel for distributed simulation.

        Args:
            local_port: Local UDP port to bind
            remote_host: Remote host IP address
            remote_port: Remote UDP port
        """
        try:
            self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.udp_socket.setblocking(False)
            self.udp_socket.bind(("0.0.0.0", local_port))

            self.remote_addr = (remote_host, remote_port)
            return True

        except Exception as e:
            print(f"Failed to connect UDP tunnel: {e}")
            return False

    def disconnect_udp_tunnel(self):
        """Disconnect from UDP tunnel."""
        if self.udp_socket is not None:
            self.udp_socket.close()
            self.udp_socket = None
            self.remote_addr = None

    # ========================================
    # Packet Injection
    # ========================================

    def inject_packet(self, data: bytes) -> bool:
        """
        Inject a packet into the receive path.

        Useful for testing micro-ROS communication.
        """
        self.receive_packet(data)
        return True

    def inject_xrcedds_message(
        self, message_type: int, payload: bytes, session_id: int = 0x01
    ) -> bool:
        """
        Inject an XRCE-DDS message for testing.

        Constructs a complete Ethernet/IP/UDP/XRCE frame.
        """
        # Build XRCE message
        xrcedds = bytearray()
        xrcedds.extend(struct.pack("!I", XRCE_DDS_MAGIC))
        xrcedds.append(session_id)  # Session ID
        xrcedds.append(0x00)  # Stream ID
        xrcedds.extend(struct.pack("!H", 0))  # Sequence

        # Add submessage
        xrcedds.append(message_type)
        xrcedds.append(0x00)  # Flags
        xrcedds.extend(struct.pack("!H", len(payload)))
        xrcedds.extend(payload)

        # Build UDP packet
        udp = bytearray()
        udp.extend(struct.pack("!H", MICRO_ROS_DEFAULT_PORT))  # Src port
        udp.extend(struct.pack("!H", MICRO_ROS_DEFAULT_PORT))  # Dst port
        udp.extend(struct.pack("!H", 8 + len(xrcedds)))  # Length
        udp.extend(struct.pack("!H", 0))  # Checksum (0 = no checksum)
        udp.extend(xrcedds)

        # Build IP packet
        ip = bytearray()
        ip.append(0x45)  # Version + IHL
        ip.append(0x00)  # TOS
        ip.extend(struct.pack("!H", 20 + len(udp)))  # Total length
        ip.extend(struct.pack("!H", 0))  # Identification
        ip.extend(struct.pack("!H", 0))  # Flags + Fragment offset
        ip.append(64)  # TTL
        ip.append(17)  # Protocol (UDP)
        ip.extend(struct.pack("!H", 0))  # Checksum (placeholder)
        ip.extend(bytes([192, 168, 1, 1]))  # Source IP
        ip.extend(bytes([192, 168, 1, 100]))  # Dest IP
        ip.extend(udp)

        # Build Ethernet frame
        eth = bytearray()
        eth.extend(self.mac_address)  # Dst MAC
        eth.extend(bytes([0x00, 0x00, 0x00, 0x00, 0x00, 0x01]))  # Src MAC
        eth.extend(struct.pack("!H", EtherType.IPV4))
        eth.extend(ip)

        return self.inject_packet(bytes(eth))

    # ========================================
    # Error Injection API
    # ========================================

    def inject_crc_error(self):
        """Inject CRC error on next received packet."""
        self._inject_crc_error = True

    def inject_frame_error(self):
        """Inject frame error on next received packet."""
        self._inject_frame_error = True

    def inject_drop_next(self):
        """Drop the next transmitted packet."""
        self._inject_drop = True

    def inject_delay(self, delay_ms: int):
        """Inject delay on next received packet."""
        self._inject_delay_ms = delay_ms

    def inject_phy_disconnect(self, duration_ms: int = 100):
        """Simulate PHY link disconnection."""

        def _reconnect():
            time.sleep(duration_ms / 1000.0)
            # Would update PHY status register

        threading.Thread(target=_reconnect, daemon=True).start()

    def inject_dma_error(self):
        """Inject DMA error condition."""
        # Would set DMA status register error bits
        pass

    # ========================================
    # Statistics API
    # ========================================

    def get_statistics(self) -> Dict:
        """Get network statistics."""
        return self.statistics.to_dict()

    def get_xrcedds_summary(self) -> Dict:
        """Get XRCE-DDS analysis summary."""
        return self.analyzer.get_summary()

    def reset_statistics(self):
        """Reset all statistics counters."""
        self.statistics.reset()
        self.analyzer = XRCEAnalyzer()

    # ========================================
    # Capture Support
    # ========================================

    def start_capture(self, filename: str):
        """Start PCAP capture."""
        # Implementation would write PCAP format
        self._capture_file = open(filename, "wb")
        # Write PCAP header
        header = struct.pack(
            "<IHHIIII",
            0xA1B2C3D4,  # Magic number
            2,
            4,  # Version
            0,  # Timezone
            65535,  # Snaplen
            1,  # Ethernet
        )
        self._capture_file.write(header)

    def stop_capture(self):
        """Stop PCAP capture."""
        if hasattr(self, "_capture_file"):
            self._capture_file.close()
            del self._capture_file


# ========================================
# Renode Integration Class
# ========================================


class EthernetHook:
    """
    Renode Python hook for Ethernet emulation.

    This class provides the integration between the Python hooks
    and the Renode simulation environment.
    """

    _instances: Dict[str, "EthernetHook"] = {}

    @classmethod
    def Create(cls, peripheral_name: str) -> "EthernetHook":
        """Create or get Ethernet hook for a peripheral."""
        if peripheral_name not in cls._instances:
            cls._instances[peripheral_name] = EthernetHook(peripheral_name)
        return cls._instances[peripheral_name]

    def __init__(self, peripheral_name: str):
        self.peripheral_name = peripheral_name
        self.interface = VirtualEthernetInterface(f"renode_{peripheral_name}")

        # Renode integration
        self._machine = None
        self._peripheral = None

    def Enable(self):
        """Enable the Ethernet hook."""
        logger.debug(
            "EthernetHook.Enable: enabling hook for '%s'", self.peripheral_name
        )
        self.interface.enable()

    def Disable(self):
        """Disable the Ethernet hook."""
        logger.debug(
            "EthernetHook.Disable: disabling hook for '%s'", self.peripheral_name
        )
        self.interface.disable()

    def SetMacAddress(self, b0: int, b1: int, b2: int, b3: int, b4: int, b5: int):
        """Set MAC address."""
        self.interface.set_mac_address(bytes([b0, b1, b2, b3, b4, b5]))

    def SetPromiscuousMode(self, enable: bool):
        """Enable/disable promiscuous mode."""
        self.interface.promiscuous = enable

    def InjectPacket(self, filename: str) -> bool:
        """Inject packet from file."""
        try:
            with open(filename, "rb") as f:
                data = f.read()
            return self.interface.inject_packet(data)
        except Exception as e:
            print(f"Failed to inject packet: {e}")
            return False

    def InjectXRCEMessage(self, message_type: int, payload_hex: str) -> bool:
        """Inject XRCE-DDS message."""
        logger.debug(
            "EthernetHook.InjectXRCEMessage: type=0x%02X, payload_len=%d bytes, hex=%s",
            message_type,
            len(payload_hex) // 2,
            payload_hex[:32] + "..." if len(payload_hex) > 32 else payload_hex,
        )
        payload = bytes.fromhex(payload_hex)
        result = self.interface.inject_xrcedds_message(message_type, payload)
        logger.debug(
            "EthernetHook.InjectXRCEMessage: injection %s",
            "succeeded" if result else "failed",
        )
        return result

    def InjectCrcError(self):
        """Inject CRC error."""
        self.interface.inject_crc_error()

    def InjectFrameError(self):
        """Inject frame error."""
        self.interface.inject_frame_error()

    def InjectDrop(self):
        """Inject packet drop."""
        self.interface.inject_drop_next()

    def InjectDelay(self, delay_ms: int):
        """Inject delay."""
        self.interface.inject_delay(delay_ms)

    def InjectPhyDisconnect(self, duration_ms: int = 100):
        """Inject PHY disconnect."""
        self.interface.inject_phy_disconnect(duration_ms)

    def GetStatistics(self) -> str:
        """Get statistics as JSON string."""
        return json.dumps(self.interface.get_statistics(), indent=2)

    def GetXRCESummary(self) -> str:
        """Get XRCE-DDS summary as JSON string."""
        return json.dumps(self.interface.get_xrcedds_summary(), indent=2)

    def ResetStatistics(self):
        """Reset statistics."""
        self.interface.reset_statistics()

    def ConnectTap(self, tap_name: str = "tap0") -> bool:
        """Connect to TAP device."""
        logger.debug(
            "EthernetHook.ConnectTap: attempting connection to TAP '%s' for '%s'",
            tap_name,
            self.peripheral_name,
        )
        result = self.interface.connect_tap(tap_name)
        if result:
            logger.debug(
                "EthernetHook.ConnectTap: successfully connected to TAP '%s'", tap_name
            )
        else:
            logger.debug(
                "EthernetHook.ConnectTap: failed to connect to TAP '%s'", tap_name
            )
        return result

    def ConnectTunnel(
        self, local_port: int, remote_host: str, remote_port: int
    ) -> bool:
        """Connect to UDP tunnel."""
        return self.interface.connect_udp_tunnel(local_port, remote_host, remote_port)

    def StartCapture(self, filename: str):
        """Start packet capture."""
        self.interface.start_capture(filename)

    def StopCapture(self):
        """Stop packet capture."""
        self.interface.stop_capture()


# ========================================
# Utility Functions
# ========================================


def create_test_packet(src_mac: bytes, dst_mac: bytes, payload: bytes) -> bytes:
    """Create a test Ethernet packet."""
    frame = bytearray()
    frame.extend(dst_mac)
    frame.extend(src_mac)
    frame.extend(struct.pack("!H", 0x0800))  # IPv4

    # Simple IP header
    ip = bytearray()
    ip.append(0x45)
    ip.append(0x00)
    total_len = 20 + len(payload)
    ip.extend(struct.pack("!H", total_len))
    ip.extend(struct.pack("!H", 0))
    ip.extend(struct.pack("!H", 0))
    ip.append(64)
    ip.append(0x11)  # UDP
    ip.extend(struct.pack("!H", 0))
    ip.extend(bytes([192, 168, 1, 1]))
    ip.extend(bytes([192, 168, 1, 100]))

    frame.extend(ip)
    frame.extend(payload)

    return bytes(frame)


def crc32(data: bytes) -> int:
    """Calculate CRC32 for Ethernet frames."""
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte << 24
        for _ in range(8):
            if crc & 0x80000000:
                crc = (crc << 1) ^ 0x04C11DB7
            else:
                crc <<= 1
    return ~crc & 0xFFFFFFFF


# ========================================
# Main Entry Point (for testing)
# ========================================

if __name__ == "__main__":
    # Test the hook
    hook = EthernetHook.Create("test_eth")
    hook.Enable()

    print("Ethernet hook enabled")
    print(f"Statistics: {hook.GetStatistics()}")

    # Inject test XRCE-DDS message
    hook.InjectXRCEMessage(XRCEMessageKind.HEARTBEAT, "01020304")

    print(f"XRCE Summary: {hook.GetXRCESummary()}")

    hook.Disable()
