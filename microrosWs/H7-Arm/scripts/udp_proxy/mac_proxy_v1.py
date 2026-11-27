#!/usr/bin/env python3
import socket, json, struct, subprocess, time

# Listen on Mac's 192.168.3.1:9999 for Jetson commands
LISTEN_IP = "192.168.3.1"
LISTEN_PORT = 9999
CLI = "/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI"

# Shared memory offsets
BASE = 0x38000000
OFF_JOINT_CMD_POS = 0x28
OFF_JOINT_CMD_SEQ = 0x58
OFF_JOINT_CMD_ACK = 0x5C

seq = 0

def write32(addr, value):
    cmd = [CLI, "-c", "port=SWD", "mode=HOTPLUG", "-w32", hex(addr), hex(value & 0xFFFFFFFF)]
    try:
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
        if result.returncode != 0:
            print(f"write32 {addr:#x} {value} failed rc={result.returncode} out={result.stdout[:200]} err={result.stderr[:200]}", flush=True)
    except Exception as e:
        print(f"write32 fail {addr:#x} {value}: {e}", flush=True)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((LISTEN_IP, LISTEN_PORT))
print(f"Mac UDP proxy listening {LISTEN_IP}:{LISTEN_PORT}, forwarding to SRAM4 via ST-Link", flush=True)
while True:
    data, addr = sock.recvfrom(4096)
    try:
        msg = json.loads(data.decode())
        # msg is list of 6 floats rad
        positions = msg if isinstance(msg, list) else msg.get("position", [])
        if len(positions) < 6:
            print(f"bad positions len {len(positions)}", flush=True)
            continue
        # convert rad to mdeg: rad * 1000 * 57.2957795
        mdegs = [int(round(p * 1000 * 57.29577951308232)) for p in positions[:6]]
        seq += 1
        print(f"recv {addr} rad {positions} -> mdeg {mdegs} seq {seq}", flush=True)
        # write positions
        for i, mdeg in enumerate(mdegs):
            write32(BASE + OFF_JOINT_CMD_POS + i*4, mdeg)
        # write seq
        write32(BASE + OFF_JOINT_CMD_SEQ, seq)
        # also clean? The CM4 will see seq increment and read positions
        print(f"written seq {seq}", flush=True)
    except Exception as e:
        print(f"err {e}", flush=True)
