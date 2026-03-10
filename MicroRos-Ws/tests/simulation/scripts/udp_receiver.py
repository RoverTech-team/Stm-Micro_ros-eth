#!/usr/bin/env python3
import argparse
import socket
import sys
import time
import json
from pathlib import Path
from datetime import datetime

DEFAULT_PORT = 12345
DEFAULT_COUNT = 1
DEFAULT_TIMEOUT = 10
DEFAULT_BUFFER_SIZE = 65507


def parse_args():
    parser = argparse.ArgumentParser(
        description="UDP packet receiver for simulation testing",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --port 12345
  %(prog)s --port 12345 --count 10 --timeout 30
  %(prog)s --port 12345 --output received.bin
  %(prog)s --port 12345 --json --count 5
  %(prog)s --port 12345 --bind-ip 192.168.1.10
""",
    )

    parser.add_argument(
        "--port",
        "-p",
        type=int,
        default=DEFAULT_PORT,
        help=f"Port to listen on (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--bind-ip",
        "-b",
        default="0.0.0.0",
        help="IP address to bind to (default: 0.0.0.0, all interfaces)",
    )
    parser.add_argument(
        "--count",
        "-c",
        type=int,
        default=DEFAULT_COUNT,
        help=f"Number of packets to receive before exiting (default: {DEFAULT_COUNT}, 0 for unlimited)",
    )
    parser.add_argument(
        "--timeout",
        "-t",
        type=float,
        default=DEFAULT_TIMEOUT,
        help=f"Socket timeout in seconds (default: {DEFAULT_TIMEOUT}, 0 for no timeout)",
    )
    parser.add_argument(
        "--buffer-size",
        type=int,
        default=DEFAULT_BUFFER_SIZE,
        help=f"Receive buffer size (default: {DEFAULT_BUFFER_SIZE})",
    )
    parser.add_argument(
        "--output",
        "-o",
        help="Output file for received data (last packet only, or all with --append)",
    )
    parser.add_argument(
        "--append",
        action="store_true",
        help="Append all packets to output file",
    )
    parser.add_argument(
        "--output-prefix",
        help="Prefix for per-packet output files (e.g., 'packet_' creates packet_001.bin, packet_002.bin...)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output results as JSON",
    )
    parser.add_argument(
        "--stats",
        action="store_true",
        help="Show statistics after receiving",
    )
    parser.add_argument(
        "--quiet",
        "-q",
        action="store_true",
        help="Suppress output except errors",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Show detailed output including packet contents",
    )
    parser.add_argument(
        "--hex",
        action="store_true",
        help="Display packet data as hexadecimal",
    )

    return parser.parse_args()


class PacketReceiver:
    def __init__(self, args):
        self.args = args
        self.sock = None
        self.packets = []
        self.total_bytes = 0
        self.errors = 0
        self.start_time = None

    def setup_socket(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        if self.args.timeout > 0:
            self.sock.settimeout(self.args.timeout)

        self.sock.bind((self.args.bind_ip, self.args.port))

        if not self.args.quiet:
            print(f"Listening on {self.args.bind_ip}:{self.args.port}")
            if self.args.count > 0:
                print(f"Waiting for {self.args.count} packet(s)...")
            else:
                print("Waiting for packets (Ctrl+C to stop)...")
            print()

    def format_data(self, data, max_len=64):
        if self.args.hex:
            return data[:max_len].hex() + ("..." if len(data) > max_len else "")

        try:
            text = data[:max_len].decode("utf-8")
            if all(c.isprintable() or c.isspace() for c in text):
                return text + ("..." if len(data) > max_len else "")
        except UnicodeDecodeError:
            pass

        return data[:max_len].hex() + ("..." if len(data) > max_len else "")

    def save_packet(self, data, packet_num):
        if self.args.output_prefix:
            filename = f"{self.args.output_prefix}{packet_num:03d}.bin"
            Path(filename).write_bytes(data)
            if self.args.verbose:
                print(f"  Saved to: {filename}")

        if self.args.output:
            mode = "ab" if self.args.append else "wb"
            with open(self.args.output, mode) as f:
                f.write(data)
            if self.args.verbose:
                print(f"  Saved to: {self.args.output}")

    def receive_packets(self):
        self.setup_socket()
        self.start_time = time.time()
        packet_count = 0
        target_count = self.args.count if self.args.count > 0 else float("inf")

        try:
            while packet_count < target_count:
                try:
                    data, addr = self.sock.recvfrom(self.args.buffer_size)
                    packet_count += 1
                    self.total_bytes += len(data)

                    packet_info = {
                        "num": packet_count,
                        "timestamp": datetime.now().isoformat(),
                        "src_ip": addr[0],
                        "src_port": addr[1],
                        "size": len(data),
                        "data": data,
                    }
                    self.packets.append(packet_info)

                    if not self.args.quiet and not self.args.json:
                        print(
                            f"[{packet_count}] Received {len(data)} bytes from {addr[0]}:{addr[1]}"
                        )
                        if self.args.verbose:
                            print(f"  Data: {self.format_data(data)}")

                    self.save_packet(data, packet_count)

                except socket.timeout:
                    if not self.args.quiet:
                        print(
                            f"Timeout: No packet received within {self.args.timeout}s"
                        )
                    self.errors += 1
                    break

        except KeyboardInterrupt:
            if not self.args.quiet:
                print("\nStopped by user")

        self.sock.close()

    def get_stats(self):
        elapsed = time.time() - self.start_time if self.start_time else 0

        stats = {
            "packets_received": len(self.packets),
            "total_bytes": self.total_bytes,
            "elapsed_time": round(elapsed, 3),
            "bytes_per_second": round(self.total_bytes / elapsed, 2)
            if elapsed > 0
            else 0,
            "errors": self.errors,
        }

        if self.packets:
            sizes = [p["size"] for p in self.packets]
            stats["avg_packet_size"] = round(sum(sizes) / len(sizes), 2)
            stats["min_packet_size"] = min(sizes)
            stats["max_packet_size"] = max(sizes)

            sources = set((p["src_ip"], p["src_port"]) for p in self.packets)
            stats["unique_sources"] = len(sources)

        return stats

    def print_stats(self):
        stats = self.get_stats()

        print("\nStatistics:")
        print(f"  Packets received: {stats['packets_received']}")
        print(f"  Total bytes: {stats['total_bytes']}")
        print(f"  Elapsed time: {stats['elapsed_time']}s")
        if stats["elapsed_time"] > 0:
            print(f"  Throughput: {stats['bytes_per_second']} bytes/s")
        if "avg_packet_size" in stats:
            print(f"  Avg packet size: {stats['avg_packet_size']} bytes")
            print(f"  Min packet size: {stats['min_packet_size']} bytes")
            print(f"  Max packet size: {stats['max_packet_size']} bytes")
            print(f"  Unique sources: {stats['unique_sources']}")
        if stats["errors"]:
            print(f"  Errors: {stats['errors']}")

    def output_json(self):
        stats = self.get_stats()
        packets_json = []

        for p in self.packets:
            packet = {
                "num": p["num"],
                "timestamp": p["timestamp"],
                "src_ip": p["src_ip"],
                "src_port": p["src_port"],
                "size": p["size"],
                "data_hex": p["data"].hex(),
            }
            try:
                packet["data_text"] = p["data"].decode("utf-8")
            except UnicodeDecodeError:
                packet["data_text"] = None
            packets_json.append(packet)

        result = {
            "stats": stats,
            "packets": packets_json,
        }

        print(json.dumps(result, indent=2))


def main():
    args = parse_args()
    receiver = PacketReceiver(args)
    receiver.receive_packets()

    if args.json:
        receiver.output_json()
    elif args.stats:
        receiver.print_stats()

    if receiver.packets:
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
