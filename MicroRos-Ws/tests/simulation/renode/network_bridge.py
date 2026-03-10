#!/usr/bin/env python3
"""
Network Bridge for Renode XRCE-DDS Simulation

This module provides a UDP bridge between Renode's simulated Ethernet and
the micro-ROS agent running on the host or in Docker. It handles:

- Forwarding XRCE-DDS packets between simulation and agent
- Packet logging and analysis
- PCAP capture support
- Error injection for testing

Usage in Renode:
    emulation LoadPythonExtension "network_bridge.py"
    bridge = NetworkBridge.Create("xrcedds_bridge")
    bridge.SetAgentAddress("172.17.0.1", 8888)
    bridge.EnableLogging(True)
"""

import socket
import struct
import threading
import time
import json
import select
import os
from typing import Optional, Dict, List, Tuple, Any, Callable
from dataclasses import dataclass, field
from collections import deque
from enum import IntEnum
from datetime import datetime
import logging

logger = logging.getLogger(__name__)


# ========================================
# Constants
# ========================================

XRCE_DDS_MAGIC = 0x58524345  # "XRCE" in big-endian
XRCE_DDS_PORT = 8888
MAX_PACKET_SIZE = 1518
PCAP_MAGIC = 0xA1B2C3D4


# ========================================
# XRCE-DDS Message Types
# ========================================


class XRCEMessageId(IntEnum):
    DATA = 0x01
    HEARTBEAT = 0x04
    ACKNACK = 0x05
    ACKNACK_FRAG = 0x06
    HEARTBEAT_FRAG = 0x07
    DATA_FRAG = 0x08
    CREATE_CLIENT = 0x20
    CREATE = 0x21
    GET_INFO = 0x22
    DELETE = 0x23
    WRITE_DATA = 0x24
    READ_DATA = 0x25
    ACKREAD = 0x26
    NACKREAD = 0x27


# ========================================
# Data Classes
# ========================================


@dataclass
class XRCEMessage:
    timestamp: float
    source: Tuple[str, int]
    destination: Tuple[str, int]
    session_id: int
    stream_id: int
    sequence: int
    submessages: List[Dict]
    raw_data: bytes

    def to_dict(self) -> Dict:
        return {
            "timestamp": self.timestamp,
            "source": f"{self.source[0]}:{self.source[1]}",
            "destination": f"{self.destination[0]}:{self.destination[1]}",
            "session_id": self.session_id,
            "stream_id": self.stream_id,
            "sequence": self.sequence,
            "submessages": self.submessages,
            "raw_length": len(self.raw_data),
        }


@dataclass
class NetworkStats:
    tx_packets: int = 0
    tx_bytes: int = 0
    rx_packets: int = 0
    rx_bytes: int = 0
    xrcedds_messages: int = 0
    tx_errors: int = 0
    rx_errors: int = 0

    def to_dict(self) -> Dict:
        return {
            "tx_packets": self.tx_packets,
            "tx_bytes": self.tx_bytes,
            "rx_packets": self.rx_packets,
            "rx_bytes": self.rx_bytes,
            "xrcedds_messages": self.xrcedds_messages,
            "tx_errors": self.tx_errors,
            "rx_errors": self.rx_errors,
        }


@dataclass
class XRCESession:
    session_id: int
    client_key: bytes
    first_seen: float
    last_seen: float
    message_count: int = 0

    def to_dict(self) -> Dict:
        return {
            "session_id": self.session_id,
            "client_key": self.client_key.hex(),
            "first_seen": self.first_seen,
            "last_seen": self.last_seen,
            "message_count": self.message_count,
        }


# ========================================
# XRCE-DDS Parser
# ========================================


class XRCEParser:
    """Parser for XRCE-DDS protocol messages."""

    SUBMESSAGE_NAMES = {
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

    @classmethod
    def parse(cls, data: bytes) -> Optional[Dict]:
        if len(data) < 4:
            logger.debug(
                "parse: data too short (%d bytes), cannot check magic", len(data)
            )
            return None

        magic = struct.unpack("!I", data[0:4])[0]
        if magic != XRCE_DDS_MAGIC:
            logger.debug(
                "parse: magic check FAILED - expected 0x%08X, got 0x%08X",
                XRCE_DDS_MAGIC,
                magic,
            )
            return None
        logger.debug("parse: magic verification SUCCESS - XRCE header found")

        result = {"magic": "XRCE", "raw_length": len(data), "timestamp": time.time()}

        if len(data) >= 12:
            result["session_id"] = data[4]
            result["stream_id"] = data[5]
            result["sequence"] = struct.unpack("!H", data[6:8])[0]
            logger.debug(
                "parse: header fields - session_id=0x%02X stream_id=0x%02X sequence=%d",
                result["session_id"],
                result["stream_id"],
                result["sequence"],
            )

            if len(data) > 12:
                result["submessages"] = cls._parse_submessages(data[12:])

        return result

    @classmethod
    def _parse_submessages(cls, data: bytes) -> List[Dict]:
        submessages = []
        offset = 0

        while offset + 4 <= len(data):
            submsg_id = data[offset]
            flags = data[offset + 1]
            length = struct.unpack("!H", data[offset + 2 : offset + 4])[0]

            submsg = {
                "id": submsg_id,
                "name": cls.SUBMESSAGE_NAMES.get(
                    submsg_id, f"UNKNOWN(0x{submsg_id:02X})"
                ),
                "flags": flags,
                "length": length,
            }

            payload_start = offset + 4
            payload_end = payload_start + length
            if payload_end <= len(data):
                submsg["payload"] = data[payload_start:payload_end].hex()

            logger.debug(
                "_parse_submessages: found submessage '%s' (id=0x%02X) flags=0x%02X length=%d",
                submsg["name"],
                submsg_id,
                flags,
                length,
            )
            submessages.append(submsg)
            offset = payload_end

        return submessages

    @classmethod
    def is_xrcedds(cls, data: bytes) -> bool:
        if len(data) < 4:
            logger.debug(
                "is_xrcedds: data too short (%d bytes) - returning False", len(data)
            )
            return False
        magic = struct.unpack("!I", data[0:4])[0]
        result = magic == XRCE_DDS_MAGIC
        logger.debug(
            "is_xrcedds: magic check result=%s (magic=0x%08X, expected=0x%08X)",
            result,
            magic,
            XRCE_DDS_MAGIC,
        )
        return result


# ========================================
# UDP Bridge
# ========================================


class UDPBridge:
    """UDP bridge between simulated network and micro-ROS agent."""

    def __init__(self, name: str = "xrcedds_bridge"):
        self.name = name
        self.enabled = False
        self.logging_enabled = False

        self.agent_address: Tuple[str, int] = ("127.0.0.1", 8888)
        self.device_address: Tuple[str, int] = ("192.168.1.100", 8888)

        self.socket: Optional[socket.socket] = None
        self.local_port = 0

        self.stats = NetworkStats()
        self.messages: deque = deque(maxlen=1000)
        self.sessions: Dict[int, XRCESession] = {}

        self._thread: Optional[threading.Thread] = None
        self._running = False

        self._capture_file = None
        self._capture_start_time = 0

        self._tx_callback: Optional[Callable] = None
        self._rx_callback: Optional[Callable] = None

        self._inject_crc_error = False
        self._inject_frame_error = False
        self._inject_drop = False
        self._inject_delay_ms = 0

    def enable(self) -> bool:
        if self.enabled:
            logger.debug("enable: bridge already enabled, returning True")
            return True

        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.socket.bind(("0.0.0.0", 0))
            self.local_port = self.socket.getsockname()[1]
            logger.debug(
                "enable: socket bound to 0.0.0.0, assigned local_port=%d",
                self.local_port,
            )
            self.socket.setblocking(False)

            self.enabled = True
            self._running = True
            self._thread = threading.Thread(target=self._receive_loop, daemon=True)
            self._thread.start()

            return True
        except Exception as e:
            logger.debug("enable: FAILED to enable bridge - %s", e)
            print(f"Failed to enable bridge: {e}")
            return False

    def disable(self):
        logger.debug("disable: closing bridge socket and stopping receive thread")
        self._running = False
        self.enabled = False

        if self._thread:
            self._thread.join(timeout=2.0)
            self._thread = None

        if self.socket:
            self.socket.close()
            self.socket = None
            logger.debug("disable: socket closed successfully")

    def send_to_agent(self, data: bytes) -> bool:
        if not self.enabled or not self.socket:
            logger.debug(
                "send_to_agent: bridge not enabled or no socket, returning False"
            )
            return False

        logger.debug(
            "send_to_agent: packet size=%d bytes, destination=%s:%d",
            len(data),
            self.agent_address[0],
            self.agent_address[1],
        )

        if self._inject_drop:
            self._inject_drop = False
            self.stats.tx_errors += 1
            logger.debug("send_to_agent: packet DROPPED (injected error)")
            return True

        try:
            is_xrce = XRCEParser.is_xrcedds(data)
            logger.debug("send_to_agent: XRCE-DDS detection result=%s", is_xrce)
            self.socket.sendto(data, self.agent_address)
            self.stats.tx_packets += 1
            self.stats.tx_bytes += len(data)
            logger.debug(
                "send_to_agent: send SUCCESS to %s:%d",
                self.agent_address[0],
                self.agent_address[1],
            )

            if self.logging_enabled:
                self._log_packet("TX", data, self.agent_address)

            if self._capture_file:
                self._write_pcap_packet(data, direction="TX")

            if is_xrce:
                self._process_xrcedds(data, direction="TX")

            return True
        except Exception as e:
            self.stats.tx_errors += 1
            logger.debug("send_to_agent: send FAILED - %s", e)
            if self.logging_enabled:
                print(f"Send error: {e}")
            return False

    def send_to_device(self, data: bytes) -> bool:
        if not self.enabled or not self.socket:
            logger.debug(
                "send_to_device: bridge not enabled or no socket, returning False"
            )
            return False

        logger.debug(
            "send_to_device: packet size=%d bytes, destination=%s:%d",
            len(data),
            self.device_address[0],
            self.device_address[1],
        )

        try:
            self.socket.sendto(data, self.device_address)
            self.stats.tx_packets += 1
            self.stats.tx_bytes += len(data)
            logger.debug(
                "send_to_device: send SUCCESS to %s:%d",
                self.device_address[0],
                self.device_address[1],
            )

            if self.logging_enabled:
                self._log_packet("TX", data, self.device_address)

            return True
        except Exception as e:
            self.stats.tx_errors += 1
            logger.debug("send_to_device: send FAILED - %s", e)
            return False

    def _receive_loop(self):
        while self._running:
            if not self.socket:
                time.sleep(0.01)
                continue

            try:
                ready, _, _ = select.select([self.socket], [], [], 0.1)
                if not ready:
                    continue

                data, addr = self.socket.recvfrom(MAX_PACKET_SIZE)
                logger.debug(
                    "_receive_loop: received packet from %s:%d, length=%d bytes",
                    addr[0],
                    addr[1],
                    len(data),
                )

                if self._inject_delay_ms > 0:
                    time.sleep(self._inject_delay_ms / 1000.0)
                    self._inject_delay_ms = 0

                self.stats.rx_packets += 1
                self.stats.rx_bytes += len(data)

                if self.logging_enabled:
                    self._log_packet("RX", data, addr)

                if self._capture_file:
                    self._write_pcap_packet(data, direction="RX", source=addr)

                if XRCEParser.is_xrcedds(data):
                    self._process_xrcedds(data, direction="RX")

                if self._rx_callback:
                    self._rx_callback(data, addr)

            except Exception as e:
                if self._running:
                    self.stats.rx_errors += 1
                    logger.debug("_receive_loop: receive error - %s", e)
                    if self.logging_enabled:
                        print(f"Receive error: {e}")

    def _process_xrcedds(self, data: bytes, direction: str):
        parsed = XRCEParser.parse(data)
        if not parsed:
            logger.debug("_process_xrcedds: failed to parse XRCE-DDS data")
            return

        self.stats.xrcedds_messages += 1

        session_id = parsed.get("session_id", 0)
        sequence = parsed.get("sequence", 0)
        submsgs = parsed.get("submessages", [])
        submsg_names = [s.get("name", "?") for s in submsgs]
        logger.debug(
            "_process_xrcedds: %s session_id=0x%02X sequence=%d submessages=%s",
            direction,
            session_id,
            sequence,
            submsg_names,
        )

        if session_id not in self.sessions:
            self.sessions[session_id] = XRCESession(
                session_id=session_id,
                client_key=b"",
                first_seen=time.time(),
                last_seen=time.time(),
            )
        else:
            self.sessions[session_id].last_seen = time.time()

        self.sessions[session_id].message_count += 1

        if self.logging_enabled:
            print(
                f"[XRCE] {direction} Session={session_id} Seq={parsed.get('sequence', 0)} "
                f"Submsgs={submsg_names}"
            )

    def _log_packet(self, direction: str, data: bytes, addr: Tuple[str, int]):
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        is_xrce = XRCEParser.is_xrcedds(data)
        tag = "[XRCE]" if is_xrce else "[UDP]"
        print(f"[{timestamp}] {tag} {direction} {addr[0]}:{addr[1]} {len(data)} bytes")

    def set_tx_callback(self, callback: Callable):
        self._tx_callback = callback

    def set_rx_callback(self, callback: Callable):
        self._rx_callback = callback

    def set_agent_address(self, host: str, port: int):
        self.agent_address = (host, port)

    def set_device_address(self, host: str, port: int):
        self.device_address = (host, port)

    def get_statistics(self) -> Dict:
        return self.stats.to_dict()

    def get_xrcedds_sessions(self) -> Dict:
        return {
            "sessions": [s.to_dict() for s in self.sessions.values()],
            "total_messages": self.stats.xrcedds_messages,
        }

    def reset_statistics(self):
        self.stats = NetworkStats()
        self.messages.clear()
        self.sessions.clear()

    def start_capture(self, filename: str):
        logger.debug("start_capture: starting PCAP capture to file '%s'", filename)
        try:
            self._capture_file = open(filename, "wb")
            self._capture_start_time = time.time()

            header = struct.pack("<IHHIIII", PCAP_MAGIC, 2, 4, 0, 65535, 1)
            self._capture_file.write(header)
            self._capture_file.flush()
            logger.debug("start_capture: PCAP header written, capture active")

            return True
        except Exception as e:
            logger.debug("start_capture: FAILED - %s", e)
            print(f"Failed to start capture: {e}")
            return False

    def stop_capture(self):
        if self._capture_file:
            self._capture_file.close()
            self._capture_file = None
            logger.debug("stop_capture: PCAP capture stopped")

    def _write_pcap_packet(
        self, data: bytes, direction: str, source: Tuple[str, int] = None
    ):
        if not self._capture_file:
            return

        ts = time.time() - self._capture_start_time
        ts_sec = int(ts)
        ts_usec = int((ts - ts_sec) * 1000000)

        header = struct.pack("<IIII", ts_sec, ts_usec, len(data), len(data))

        self._capture_file.write(header)
        self._capture_file.write(data)
        self._capture_file.flush()

    def inject_crc_error(self):
        self._inject_crc_error = True

    def inject_frame_error(self):
        self._inject_frame_error = True

    def inject_drop(self):
        self._inject_drop = True

    def inject_delay(self, delay_ms: int):
        self._inject_delay_ms = delay_ms

    def disconnect(self):
        self._running = False

    def connect(self):
        if not self._running:
            self._running = True
            self._thread = threading.Thread(target=self._receive_loop, daemon=True)
            self._thread.start()


# ========================================
# Renode Integration Class
# ========================================


class NetworkBridge:
    """
    Renode Python hook for network bridging.

    Provides integration between the Python bridge and Renode simulation.
    """

    _instances: Dict[str, "NetworkBridge"] = {}

    @classmethod
    def Create(cls, name: str) -> "NetworkBridge":
        if name not in cls._instances:
            cls._instances[name] = NetworkBridge(name)
        return cls._instances[name]

    def __init__(self, name: str):
        self.name = name
        self.bridge = UDPBridge(name)
        self._machine = None
        self._peripheral = None

    def Enable(self) -> bool:
        logger.debug("Enable: enabling network bridge '%s'", self.name)
        return self.bridge.enable()

    def Disable(self):
        self.bridge.disable()

    def SetAgentAddress(self, host: str, port: int):
        logger.debug("SetAgentAddress: setting agent address to %s:%d", host, port)
        self.bridge.set_agent_address(host, port)

    def SetDeviceAddress(self, host: str, port: int):
        logger.debug("SetDeviceAddress: setting device address to %s:%d", host, port)
        self.bridge.set_device_address(host, port)

    def EnableLogging(self, enable: bool):
        logger.debug("EnableLogging: setting logging state to %s", enable)
        self.bridge.logging_enabled = enable

    def SendToAgent(self, data_hex: str) -> bool:
        data = bytes.fromhex(data_hex)
        return self.bridge.send_to_agent(data)

    def SendToDevice(self, data_hex: str) -> bool:
        data = bytes.fromhex(data_hex)
        return self.bridge.send_to_device(data)

    def GetStatistics(self) -> str:
        return json.dumps(self.bridge.get_statistics(), indent=2)

    def GetXRCESessions(self) -> str:
        return json.dumps(self.bridge.get_xrcedds_sessions(), indent=2)

    def ResetStatistics(self):
        self.bridge.reset_statistics()

    def StartCapture(self, filename: str) -> bool:
        return self.bridge.start_capture(filename)

    def StopCapture(self):
        self.bridge.stop_capture()

    def InjectCrcError(self):
        self.bridge.inject_crc_error()

    def InjectFrameError(self):
        self.bridge.inject_frame_error()

    def InjectDrop(self):
        self.bridge.inject_drop()

    def InjectDelay(self, delay_ms: int):
        self.bridge.inject_delay(delay_ms)

    def Disconnect(self):
        self.bridge.disconnect()

    def Connect(self):
        self.bridge.connect()

    def GetLocalPort(self) -> int:
        return self.bridge.local_port


# ========================================
# Test Entry Point
# ========================================

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="XRCE-DDS Network Bridge")
    parser.add_argument("--agent-host", default="127.0.0.1", help="Agent host")
    parser.add_argument("--agent-port", type=int, default=8888, help="Agent port")
    parser.add_argument("--device-host", default="192.168.1.100", help="Device host")
    parser.add_argument("--device-port", type=int, default=8888, help="Device port")
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Enable verbose logging"
    )
    parser.add_argument("--capture", help="PCAP capture file")

    args = parser.parse_args()

    bridge = NetworkBridge.Create("test_bridge")
    bridge.Enable()
    bridge.SetAgentAddress(args.agent_host, args.agent_port)
    bridge.SetDeviceAddress(args.device_host, args.device_port)
    bridge.EnableLogging(args.verbose)

    if args.capture:
        bridge.StartCapture(args.capture)

    print(f"Network bridge running")
    print(f"  Agent: {args.agent_host}:{args.agent_port}")
    print(f"  Device: {args.device_host}:{args.device_port}")
    print(f"  Local port: {bridge.GetLocalPort()}")
    print("Press Ctrl+C to stop")

    try:
        while True:
            time.sleep(1)
            stats = bridge.GetStatistics()
            print(f"Stats: {stats}")
    except KeyboardInterrupt:
        print("\nStopping...")

    if args.capture:
        bridge.StopCapture()

    bridge.Disable()
