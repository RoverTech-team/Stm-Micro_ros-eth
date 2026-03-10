# E2E Linux Tests

End-to-end tests for TAP-based Renode simulation on Linux.

## Prerequisites

### System Requirements
- Linux kernel with TUN/TAP support (`/dev/net/tun`)
- Root privileges or CAP_NET_ADMIN capability
- Docker (for micro-ROS agent)
- Renode simulation environment

### Install Dependencies

```bash
# Create virtual environment
python -m venv .venv
source .venv/bin/activate

# Install test dependencies
pip install -r tests/e2eLinux/requirements.txt
```

## Important: Renode Installation

The project includes `Renode.app` bundled for **macOS only**. This will NOT work on Linux.

### Installing Renode on Linux

**Option 1: Package Manager (recommended)**
```bash
# Ubuntu/Debian
sudo apt-add-repository ppa:antmicro/renode
sudo apt update
sudo apt install renode

# Fedora
sudo dnf install renode

# Arch Linux (AUR)
yay -S renode
```

**Option 2: Download from Releases**
```bash
# Download latest release
wget https://github.com/renode/renode/releases/download/v1.14.0/renode-1.14.0.linux-portable.tar.gz
tar xzf renode-1.14.0.linux-portable.tar.gz
sudo mv renode /opt/
export RENODE_PATH=/opt/renode/renode
```

**Option 3: Set RENODE_PATH**
If Renode is installed elsewhere, set the environment variable:
```bash
export RENODE_PATH=/path/to/your/renode
```

### Verify Renode Installation
```bash
which renode && renode --version
```

If Renode is not found, tests will be skipped with installation instructions.

### Build Firmware

Ensure the STM32 firmware is built:
```bash
# From project root
cd Micro_ros_eth/microroseth/Makefile/CM7
make
```

## Running Tests

### With sudo (required for TAP)
```bash
sudo .venv/bin/python -m pytest tests/e2eLinux/ -v --tap
```

### Run specific test
```bash
sudo .venv/bin/python -m pytest tests/e2eLinux/test_xrcedds_transport.py -v --tap
```

### View real-time logs
```bash
# In another terminal
tail -f tests/e2eLinux/logs/e2e_debug_*.log
```

## Network Topology

```
Linux Host
    |
    |-- tap0 (192.168.0.1)
    |       |
    |       |-- IP alias: 192.168.0.8 (agent)
    |       |
    |       |-- Renode simulation
    |           |-- STM32 (192.168.0.3)
    |               |
    |               |-- XRCE-DDS UDP -> Agent:8888
    |
    |-- Docker: micro-ROS agent (host network)
```

## Troubleshooting

### Tests skipped: Renode not found
Tests will skip if Renode is not installed. The bundled Renode.app is for macOS only.
See "Important: Renode Installation" section above.

### TAP device creation fails
```bash
# Check TUN module is loaded
lsmod | grep tun
sudo modprobe tun

# Check /dev/net/tun exists
ls -la /dev/net/tun
```

### Renode not found
```bash
# Install Renode or set path
which renode || echo "Renode not in PATH"
export RENODE_PATH=/path/to/renode
```

### Permission denied
```bash
# Run with sudo
sudo .venv/bin/python -m pytest tests/e2eLinux/ --tap -v
```

## Log Files

Logs are saved in real-time to:
- `tests/e2eLinux/logs/e2e_debug_YYYYMMDD_HHMMSS.log`

If tests hang, check the log file to see where it stopped.