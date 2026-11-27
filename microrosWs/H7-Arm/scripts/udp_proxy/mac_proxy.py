#!/usr/bin/env python3
import socket, json, subprocess, struct, time, os, tempfile

LISTEN_IP = "192.168.3.1"
LISTEN_PORT = 9999
CLI = "/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI"
BASE = 0x38000000
OFF_JOINT_CMD_POS = 0x28
OFF_JOINT_CMD_SEQ = 0x58

seq = 0

def write_block(addr, data_bytes):
    with tempfile.NamedTemporaryFile(delete=False, suffix='.bin') as f:
        f.write(data_bytes)
        fname = f.name
    cmd = [CLI, "-c", "port=SWD", "mode=HOTPLUG", "-w", fname, hex(addr)]
    try:
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
        if result.returncode != 0:
            print(f"write_block {addr:#x} failed rc={result.returncode} err={result.stderr[:500].decode(errors='ignore')}", flush=True)
        else:
            print(f"write_block {addr:#x} {len(data_bytes)}B ok", flush=True)
    except Exception as e:
        print(f"write_block fail {addr:#x}: {e}", flush=True)
    finally:
        try: os.unlink(fname)
        except: pass
    time.sleep(0.2)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((LISTEN_IP, LISTEN_PORT))
print(f"Mac UDP proxy v2 listening {LISTEN_IP}:{LISTEN_PORT}", flush=True)
while True:
    data, addr = sock.recvfrom(4096)
    try:
        msg = json.loads(data.decode())
        positions = msg if isinstance(msg, list) else msg.get("position", [])
        if len(positions) < 6:
            print(f"bad len {len(positions)}", flush=True)
            continue
        mdegs = [int(round(p * 1000 * 57.29577951308232)) for p in positions[:6]]
        seq += 1
        cur = seq
        print(f"recv {addr} rad {positions} -> mdeg {mdegs} seq {cur}", flush=True)
        pos_bytes = struct.pack('<6i', *mdegs)
        seq_bytes = struct.pack('<I', cur)
        write_block(BASE + OFF_JOINT_CMD_POS, pos_bytes)
        write_block(BASE + OFF_JOINT_CMD_SEQ, seq_bytes)
        print(f"written seq {cur}", flush=True)
    except Exception as e:
        print(f"err {e}", flush=True)
        import traceback; traceback.print_exc()
