#!/usr/bin/env python3
import argparse
import socket
import sys
import time
from pathlib import Path

DEFAULT_IP = "192.168.1.100"
DEFAULT_PORT = 12345
DEFAULT_COUNT = 1
DEFAULT_DELAY = 0.1
DEFAULT_TIMEOUT = 5
MAX_PACKET_SIZE = 65507


def parse_args():
    parser = argparse.ArgumentParser(
        description="UDP packet sender for simulation testing",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --ip 192.168.1.100 --port 12345 --data "Hello World"
  %(prog)s --ip 192.168.1.100 --port 12345 --file payload.bin
  %(prog)s --ip 192.168.1.100 --port 12345 --data "test" --count 10 --delay 0.5
  %(prog)s --ip 192.168.1.100 --port 12345 --hex "48454c4c4f"
""",
    )

    parser.add_argument(
        "--ip",
        "-i",
        default=DEFAULT_IP,
        help=f"Destination IP address (default: {DEFAULT_IP})",
    )
    parser.add_argument(
        "--port",
        "-p",
        type=int,
        default=DEFAULT_PORT,
        help=f"Destination port (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--data",
        "-d",
        help="Text data to send",
    )
    parser.add_argument(
        "--hex",
        help="Hexadecimal data to send (e.g., '48454c4c4f' for 'HELLO')",
    )
    parser.add_argument(
        "--file",
        "-f",
        help="Path to file containing data to send",
    )
    parser.add_argument(
        "--count",
        "-c",
        type=int,
        default=DEFAULT_COUNT,
        help=f"Number of packets to send (default: {DEFAULT_COUNT})",
    )
    parser.add_argument(
        "--delay",
        type=float,
        default=DEFAULT_DELAY,
        help=f"Delay between packets in seconds (default: {DEFAULT_DELAY})",
    )
    parser.add_argument(
        "--timeout",
        "-t",
        type=float,
        default=DEFAULT_TIMEOUT,
        help=f"Socket timeout in seconds (default: {DEFAULT_TIMEOUT})",
    )
    parser.add_argument(
        "--size",
        "-s",
        type=int,
        help="Generate random data of specified size (bytes)",
    )
    parser.add_argument(
        "--bind-ip",
        help="Local IP to bind to (for multi-interface systems)",
    )
    parser.add_argument(
        "--bind-port",
        type=int,
        default=0,
        help="Local port to bind to (default: 0, auto-assign)",
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
        help="Show detailed output",
    )

    return parser.parse_args()


def get_data(args):
    data = None
    source = None

    if args.data:
        data = args.data.encode("utf-8")
        source = "command line text"
    elif args.hex:
        try:
            data = bytes.fromhex(args.hex)
            source = "command line hex"
        except ValueError as e:
            print(f"Error: Invalid hexadecimal data: {e}", file=sys.stderr)
            sys.exit(1)
    elif args.file:
        try:
            file_path = Path(args.file)
            if not file_path.exists():
                print(f"Error: File not found: {args.file}", file=sys.stderr)
                sys.exit(1)
            data = file_path.read_bytes()
            source = f"file: {args.file}"
        except Exception as e:
            print(f"Error reading file: {e}", file=sys.stderr)
            sys.exit(1)
    elif args.size:
        import random

        data = bytes(random.randint(0, 255) for _ in range(args.size))
        source = f"random ({args.size} bytes)"
    else:
        data = b"test"
        source = "default"

    if len(data) > MAX_PACKET_SIZE:
        print(
            f"Error: Data size ({len(data)} bytes) exceeds maximum UDP packet size ({MAX_PACKET_SIZE} bytes)",
            file=sys.stderr,
        )
        sys.exit(1)

    return data, source


def send_packets(args):
    data, source = get_data(args)

    if not args.quiet:
        print(f"UDP Sender Configuration:")
        print(f"  Destination: {args.ip}:{args.port}")
        print(f"  Data source: {source}")
        print(f"  Data size: {len(data)} bytes")
        print(f"  Packet count: {args.count}")
        print(f"  Delay: {args.delay}s")
        print()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(args.timeout)

    if args.bind_ip or args.bind_port:
        bind_ip = args.bind_ip or "0.0.0.0"
        sock.bind((bind_ip, args.bind_port))
        if args.verbose:
            local_addr = sock.getsockname()
            print(f"Bound to {local_addr[0]}:{local_addr[1]}")

    sent_count = 0
    total_bytes = 0
    errors = []

    try:
        for i in range(args.count):
            try:
                bytes_sent = sock.sendto(data, (args.ip, args.port))
                sent_count += 1
                total_bytes += bytes_sent

                if args.verbose:
                    print(
                        f"[{i + 1}/{args.count}] Sent {bytes_sent} bytes to {args.ip}:{args.port}"
                    )

                if i < args.count - 1 and args.delay > 0:
                    time.sleep(args.delay)

            except socket.timeout:
                errors.append(f"Packet {i + 1}: Timeout")
            except socket.error as e:
                errors.append(f"Packet {i + 1}: {e}")

    except KeyboardInterrupt:
        print("\nInterrupted by user", file=sys.stderr)
    finally:
        sock.close()

    if not args.quiet:
        print(f"\nResults:")
        print(f"  Packets sent: {sent_count}/{args.count}")
        print(f"  Total bytes: {total_bytes}")
        if errors:
            print(f"  Errors: {len(errors)}")
            for error in errors:
                print(f"    - {error}")

    return 0 if sent_count == args.count and not errors else 1


def main():
    args = parse_args()
    sys.exit(send_packets(args))


if __name__ == "__main__":
    main()
