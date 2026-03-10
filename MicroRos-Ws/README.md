# MICRO_ROS_ETH

[![Version](https://img.shields.io/badge/version-1.0.0-blue)]()
[![License](https://img.shields.io/badge/license-Apache%202.0-orange)](LICENSE)
[![ROS2](https://img.shields.io/badge/ROS%202-Humble%2FJazzy-blueviolet)]()
[![Platform](https://img.shields.io/badge/platform-STM32H7-green)]()
[![CI](https://github.com/GiulioMastromartino/MICRO_ROS_ETH/workflows/CI/badge.svg)]()

A complete framework for Ethernet-based micro-ROS deployment on STM32H7 dual-core microcontrollers with simulation, testing, and monitoring infrastructure.

## Overview

**MICRO_ROS_ETH** provides an end-to-end solution for deploying ROS 2 nodes on STM32H7 microcontrollers over Ethernet. It bridges embedded systems with ROS 2 ecosystems through the XRCE-DDS protocol, enabling real-time distributed robotics applications.

### Key Features

- **Dual-Core STM32H7 Support**: Cortex-M7 (main) + Cortex-M4 (auxiliary) with hardware semaphore synchronization
- **Ethernet Transport**: UDP-based XRCE-DDS communication via LwIP stack
- **micro-ROS Integration**: Full ROS 2 client support with static memory allocation
- **Web Dashboard**: Real-time monitoring and control via Flask application
- **Hardware Simulation**: Renode-based virtual platform for development without physical hardware
- **Comprehensive Testing**: Unit, integration, fuzzing, and end-to-end test suites

### Target Use Case

Ideal for:
- Distributed robotic systems requiring real-time communication
- Industrial IoT with ROS 2 integration
- Edge computing nodes with limited resources
- Educational platforms for embedded ROS 2 development

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              SYSTEM ARCHITECTURE                                 │
└─────────────────────────────────────────────────────────────────────────────────┘

    ┌──────────────────────────────────────────────────────────────────────┐
    │                         DEVELOPMENT HOST                              │
    │  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────────────┐  │
    │  │   MicroK3       │  │  micro-ROS      │  │   Renode Simulator   │  │
    │  │   Dashboard     │  │  Agent (Docker) │  │   (Virtual STM32)    │  │
    │  │   (Flask)       │  │   UDP:8888      │  │                      │  │
    │  │   :5050         │  │                 │  │                      │  │
    │  └────────┬────────┘  └────────┬────────┘  └──────────┬───────────┘  │
    │           │                    │                      │              │
    │           │   ROS2 Topics      │   XRCE-DDS/UDP       │ TAP/Bridge  │
    │           └────────────────────┼──────────────────────┼──────────────┤
    └─────────────────────────────────┼──────────────────────┼──────────────┘
                                      │                      │
                                      │         ┌────────────┘
                                      │         │
                              ┌───────▼─────────▼───────┐
                              │     NETWORK (LAN)        │
                              │     192.168.0.0/24       │
                              └───────────┬─────────────┘
                                          │
    ┌─────────────────────────────────────┼─────────────────────────────────────┐
    │                         EMBEDDED TARGET                               │
    │  ┌───────────────────────────────────▼───────────────────────────────┐   │
    │  │                    STM32H755/STM32H743                            │   │
    │  │  ┌─────────────────────┐    ┌─────────────────────┐               │   │
    │  │  │    Cortex-M7        │    │    Cortex-M4        │               │   │
    │  │  │   (480 MHz)         │    │   (240 MHz)         │               │   │
    │  │  │                     │    │                     │               │   │
    │  │  │ ┌─────────────────┐ │    │                     │               │   │
    │  │  │ │    FreeRTOS     │ │    │                     │  HSEM Sync    │   │
    │  │  │ └────────┬────────┘ │    │                     ◄─────────────►│   │
    │  │  │          │          │    │                     │               │   │
    │  │  │ ┌────────▼────────┐ │    └─────────────────────┘               │   │
    │  │  │ │    LwIP Stack   │ │                                            │   │
    │  │  │ └────────┬────────┘ │                                            │   │
    │  │  │          │          │                                            │   │
    │  │  │ ┌────────▼────────┐ │                                            │   │
    │  │  │ │  micro-ROS      │ │                                            │   │
    │  │  │ │  XRCE-DDS       │ │                                            │   │
    │  │  │ └────────┬────────┘ │                                            │   │
    │  │  └───────────┼──────────┘                                            │   │
    │  │              │                                                        │   │
    │  │  ┌───────────▼──────────┐                                            │   │
    │  │  │   Ethernet MAC/PHY   │                                            │   │
    │  │  │   (LAN8742A)         │                                            │   │
    │  │  └──────────────────────┘                                            │   │
    │  └──────────────────────────────────────────────────────────────────────┘   │
    └─────────────────────────────────────────────────────────────────────────────┘

DATA FLOW:
┌─────────┐    UDP:8888    ┌─────────┐    ROS2    ┌───────────┐
│ STM32H7  │ ────────────► │  Agent  │ ─────────► │ Dashboard │
│ (Client) │ ◄──────────── │ (DDS)   │ ◄───────── │ (Monitor) │
└─────────┘  XRCE-DDS      └─────────┘  Topics    └───────────┘
```

---

## Project Structure

```
MICRO_ROS_ETH/
├── Micro_ros_eth/                    # STM32H7 firmware with micro-ROS
│   └── microroseth/
│       ├── CM7/                      # Cortex-M7 core (main firmware)
│       │   ├── Core/                  # HAL initialization, main()
│       │   │   ├── Src/               # Source files
│       │   │   │   ├── main.c         # Entry point, MPU config
│       │   │   │   ├── freertos.c     # FreeRTOS tasks
│       │   │   │   ├── eth.c          # Ethernet HAL
│       │   │   │   └── gpio.c         # GPIO configuration
│       │   │   └── Inc/               # Headers
│       │   └── LWIP/                  # LwIP configuration
│       │       ├── App/               # LwIP application layer
│       │       └── Target/            # Ethernet interface driver
│       ├── CM4/                       # Cortex-M4 core (auxiliary)
│       │   └── Core/                  # Secondary core code
│       ├── Common/                    # Shared code between cores
│       │   └── Src/                   # Common sources
│       ├── Drivers/                   # HAL & CMSIS drivers
│       │   ├── BSP/                   # Board support (LAN8742A)
│       │   ├── CMSIS/                 # CMSIS core/device
│       │   └── STM32H7xx_HAL_Driver/  # STM32 HAL
│       ├── Middlewares/               # Third-party middleware
│       │   └── Third_Party/
│       │       └── FreeRTOS/          # FreeRTOS source
│       ├── micro_ros_stm32cubemx_utils/  # micro-ROS utilities
│       │   ├── extra_sources/         # Custom transports, allocators
│       │   └── microros_static_library/  # Pre-built micro-ROS lib
│       └── Makefile/                  # Build system
│           ├── Makefile               # Root Makefile
│           ├── CM7/Makefile           # CM7 build
│           └── CM4/Makefile           # CM4 build
│
├── microk3/                           # Flask dashboard for ROS2 monitoring
│   ├── app.py                         # Main Flask application
│   ├── config.py                      # Configuration management
│   ├── ros_interface.py               # ROS2 bridge (rclpy)
│   ├── models/                        # Data models
│   │   └── node.py                    # Node representation
│   ├── templates/                     # HTML templates (Jinja2)
│   │   ├── base.html                  # Base layout
│   │   ├── index.html                 # Dashboard home
│   │   ├── node.html                  # Node details
│   │   ├── failures.html              # Failure history
│   │   ├── network.html               # Network status
│   │   ├── configuration.html         # Settings (auth required)
│   │   └── logs.html                  # Log viewer
│   ├── docker-compose.yml             # Docker orchestration
│   ├── requirements.txt               # Python dependencies
│   └── Dockerfile                     # Container definition
│
├── tests/
│   ├── e2e/                           # End-to-end tests (macOS)
│   │   ├── test_dashboard_api.py      # API endpoint tests
│   │   ├── test_e2e_pipeline.py       # Full pipeline tests
│   │   ├── test_xrcedds_transport.py  # XRCE-DDS transport
│   │   ├── mock_stm32_client.py        # Simulated STM32 client
│   │   └── requirements.txt           # Test dependencies
│   │
│   ├── e2eLinux/                      # End-to-end tests (Linux)
│   │   ├── test_xrcedds_transport.py  # TAP-based transport tests
│   │   └── README.md                  # Linux-specific setup
│   │
│   ├── integration/                   # Integration tests
│   │   ├── test_microros_agent.py     # Agent communication
│   │   ├── test_ros2_communication.py # ROS2 topic tests
│   │   ├── test_udp_transport.py      # UDP layer tests
│   │   └── requirements.txt           # Test dependencies
│   │
│   ├── simulation/                    # Renode simulation scripts
│   │   ├── renode/                    # Platform files
│   │   │   ├── stm32h755.repl         # Basic platform
│   │   │   ├── stm32h755_networked.repl # With network
│   │   │   ├── stm32h755_tap.repl     # TAP interface
│   │   │   ├── ethernet.py            # Ethernet peripheral
│   │   │   └── network_bridge.py      # Network bridging
│   │   ├── robot/                     # Robot Framework tests
│   │   │   ├── variables.py           # Test variables
│   │   │   └── variables_tap.py       # TAP-specific vars
│   │   └── scripts/                   # Helper scripts
│   │       ├── udp_receiver.py        # UDP test receiver
│   │       ├── udp_sender.py          # UDP test sender
│   │       └── run_renode.sh          # Simulation runner
│   │
│   ├── host/                          # Host-based unit tests
│   │   ├── test_memory.c              # Memory management tests
│   │   ├── test_ip_config.c           # IP configuration tests
│   │   ├── test_transports.c          # Transport layer tests
│   │   ├── test_udp_transport.c       # UDP transport tests
│   │   ├── test_microros_allocators.c # Allocator tests
│   │   ├── mocks/                     # Mock implementations
│   │   │   ├── mock_lwip.c            # LwIP mocks
│   │   │   ├── mock_freertos.c        # FreeRTOS mocks
│   │   │   ├── mock_hal.c             # HAL mocks
│   │   │   └── mock_udp_transport.c  # UDP mocks
│   │   ├── unity/                     # Unity test framework
│   │   └── Makefile                   # Test build system
│   │
│   ├── fuzzing/                       # Fuzz testing
│   │   ├── fuzz_udp_transport.c       # UDP fuzzer
│   │   ├── fuzz_ethernet_frame.c      # Ethernet frame fuzzer
│   │   ├── fuzz_microros_allocators.c # Allocator fuzzer
│   │   ├── fuzz_test_harness.c        # Standalone harness
│   │   ├── corpus/                    # Seed inputs
│   │   └── Makefile                   # Fuzzer build
│   │
│   └── docker/                        # Docker test environment
│       └── docker-compose.yml         # Test containers
│
├── Renode.app/                        # Renode simulator (macOS only)
│   └── Contents/MacOS/                # Application binaries
│
└── .github/                           # CI/CD workflows
    └── workflows/
        └── ci.yml                     # Main CI pipeline
```

### Key Files Explained

| File | Purpose |
|------|---------|
| `Micro_ros_eth/microroseth/CM7/Core/Src/main.c` | Firmware entry point, MPU config, dual-core sync |
| `Micro_ros_eth/microroseth/CM7/LWIP/Target/ethernetif.c` | LwIP ethernet interface |
| `Micro_ros_eth/microroseth/Makefile/CM7/Makefile` | Firmware build configuration |
| `microk3/app.py` | Flask dashboard application |
| `microk3/docker-compose.yml` | Dashboard + micro-ROS agent orchestration |
| `tests/host/Makefile` | Unit test build with Unity framework |
| `tests/fuzzing/Makefile` | LibFuzzer build configuration |
| `.github/workflows/ci.yml` | CI/CD pipeline definition |

---

## Hardware Requirements

### Supported STM32H7 Boards

| Board | MCU | Ethernet PHY | Notes |
|-------|-----|--------------|-------|
| STM32H755I-EV | STM32H755XIH6 | LAN8742A | Primary target |
| STM32H743I-EVAL | STM32H743XIH6 | LAN8742A | Single-core variant |
| STM32H747I-DISCO | STM32H747XIH6 | LAN8742A | Dual-core discovery |
| NUCLEO-H743ZI2 | STM32H743ZIT6 | LAN8742A | Nucleo board |

### Network Requirements

- **Ethernet**: 10/100 Mbps connection
- **Network**: Dedicated LAN segment recommended (192.168.0.0/24)
- **Agent Host**: Ubuntu 22.04+ or macOS 12+ with Docker

### Development Hardware

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| ST-Link/V2 | Required | ST-Link/V3 |
| Ethernet Switch | 100 Mbps | Gigabit managed |
| Debug Probe | ST-Link | ST-Link/V3 + JTAG |
| Power Supply | 5V/500mA | 5V/1A |

---

## Software Requirements

### Operating Systems

| OS | Version | Support Level |
|----|---------|---------------|
| Ubuntu | 22.04 LTS | Full (CI tested) |
| macOS | 13+ (Ventura) | Full (development) |
| Windows | 10/11 WSL2 | Partial (firmware only) |

### Toolchains

| Tool | Version | Purpose |
|------|---------|---------|
| ARM GCC | 10.3+ | Firmware compilation |
| STM32CubeMX | 6.9+ | Project configuration |
| STM32CubeIDE | 1.13+ | IDE (optional) |
| Docker | 24.0+ | Container runtime |
| Python | 3.9+ | Dashboard & tests |
| Make | 4.3+ | Build system |

### Dependencies

**Firmware:**
```bash
# ARM toolchain (Ubuntu)
sudo apt install gcc-arm-none-eabi

# STM32CubeMX (download from st.com)
# Docker (for micro-ROS library builder)
```

**Dashboard:**
```bash
pip install -r microk3/requirements.txt
# Docker Compose for full stack
```

**Testing:**
```bash
# Python test dependencies
pip install pytest pytest-asyncio rclpy std_msgs

# Renode (Linux)
sudo apt-add-repository ppa:antmicro/renode
sudo apt install renode

# Static analysis
sudo apt install cppcheck clang-tidy
```

---

## Quick Start

### Prerequisites Check

```bash
# Check ARM toolchain
arm-none-eabi-gcc --version

# Check Docker
docker --version
docker-compose --version

# Check Python
python3 --version  # Should be 3.9+

# Check Renode (macOS)
ls Renode.app/Contents/MacOS/renode

# Check Renode (Linux)
which renode
```

### Clone and Build

```bash
# Clone repository
git clone https://github.com/GiulioMastromartino/MICRO_ROS_ETH.git
cd MICRO_ROS_ETH

# Build firmware (requires micro-ROS library)
cd Micro_ros_eth/microroseth/Makefile/CM7
make

# Or build micro-ROS library first
docker pull microros/micro_ros_static_library_builder:humble
# Follow prompts to configure library
```

### Run Dashboard

```bash
cd microk3

# Configure environment
cp .env.example .env
python3 -c "import secrets; print(f'SECRET_KEY={secrets.token_hex(32)}')" >> .env

# Start with Docker Compose
docker-compose up --build

# Access dashboard
open http://localhost:5050
```

### Flash Firmware

```bash
# Using OpenOCD
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build/MicroRosEth.elf verify reset exit"

# Using STM32CubeProgrammer CLI
STM32_Programmer_CLI -c port=SWD -w build/MicroRosEth.elf 0x08000000 -v -rst
```

---

## Firmware Development

### Building Firmware

```bash
cd Micro_ros_eth/microroseth/Makefile/CM7

# Clean build
make clean && make -j$(nproc)

# Output files
ls -la build/
# MicroRosEth.elf  - Debug symbol file
# MicroRosEth.bin  - Raw binary
# MicroRosEth.hex  - Intel HEX
```

### STM32CubeMX Configuration

Key settings in `MicroRosEth.ioc`:

| Category | Setting | Value |
|----------|---------|-------|
| **Ethernet** | Mode | RMII |
| | PHY Address | 0 |
| **LwIP** | DHCP | Disabled |
| | IP Address | 192.168.0.3 |
| | Netmask | 255.255.255.0 |
| | UDP | Enabled |
| | MEMP_NUM_UDP_PCB | 15 |
| | LWIP_SO_RCVTIMEO | Enabled |
| **FreeRTOS** | Heap Size | 64KB minimum |
| | Total Heap | 128KB |
| | micro-ROS Task Stack | 10KB+ |
| **MPU** | Region 0 | SRAM1 (eth descriptors) |

### micro-ROS Setup

```bash
# Generate static library for your target
cd Micro_ros_eth/microroseth

docker pull microros/micro_ros_static_library_builder:humble
docker run -it --rm \
  -v $(pwd):/project \
  --env MICROROS_LIBRARY_FOLDER=micro_ros_stm32cubemx_utils/microros_static_library \
  microros/micro_ros_static_library_builder:humble
```

### Ethernet Configuration

Critical STM32H7 Ethernet settings:

```c
// In main.c - D-Cache MUST be disabled for DMA stability
SCB_DisableDCache();

// MPU configuration for Ethernet DMA buffers
MPU_Region_InitTypeDef MPU_InitStruct = {0};
MPU_InitStruct.Enable = MPU_REGION_ENABLE;
MPU_InitStruct.BaseAddress = 0x30000000;  // SRAM2
MPU_InitStruct.Size = MPU_REGION_SIZE_32KB;
MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
HAL_MPU_ConfigRegion(&MPU_InitStruct);
```

---

## Dashboard (microk3)

### Features

- **Real-time Monitoring**: Node health, status, uptime
- **Failure Management**: Automatic detection and logging
- **Task Distribution**: Visual task allocation view
- **Network Status**: Connection topology display
- **REST API**: Programmatic access with authentication
- **ROS 2 Native**: Direct topic subscription via rclpy

### Installation

**Option 1: Docker (Recommended)**

```bash
cd microk3
docker-compose up --build
```

**Option 2: Native (requires ROS 2)**

```bash
# Install ROS 2 Humble first
source /opt/ros/humble/setup.bash

cd microk3
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python app.py
```

### Configuration

Environment variables (`.env`):

| Variable | Default | Description |
|----------|---------|-------------|
| `SECRET_KEY` | Required | Flask session secret |
| `ADMIN_USERNAME` | admin | Dashboard admin user |
| `ADMIN_PASSWORD` | Required | Admin password |
| `LOG_LEVEL` | INFO | Logging verbosity |
| `ROS_DOMAIN_ID` | 0 | ROS 2 domain |
| `FLASK_HOST` | 127.0.0.1 | Bind address |
| `FLASK_PORT` | 5050 | Server port |

### API Documentation

**Authentication**: HTTP Basic Auth required for write operations.

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/system_status` | GET | No | System overview |
| `/api/nodes` | GET | No | List all nodes |
| `/api/nodes/<id>` | GET | No | Node details |
| `/api/failures` | GET | No | Failure history |
| `/api/tasks` | GET | No | Task status |
| `/api/update_node` | POST | Yes | Update node status |
| `/api/add_failure` | POST | Yes | Log failure |
| `/health` | GET | No | Health check |

**Example API Usage:**

```bash
# Get system status
curl http://localhost:5050/api/system_status

# Update node (requires auth)
curl -u admin:password \
  -X POST \
  -H "Content-Type: application/json" \
  -d '{"node_id": 1, "status": "standby"}' \
  http://localhost:5050/api/update_node
```

### ROS 2 Topics

| Topic | Type | Direction | Description |
|-------|------|-----------|-------------|
| `/microk3/node_status` | `std_msgs/String` | Subscribe | Node health updates |
| `/microk3/system_alerts` | `std_msgs/String` | Subscribe | Failure alerts |
| `/microk3/commands` | `std_msgs/String` | Publish | Node commands |

**Message Format (JSON in `data` field):**

```json
{
  "id": 1,
  "status": "active",
  "health": 95,
  "uptime": "12h 34m"
}
```

---

## Testing

### Test Types Overview

| Type | Location | Framework | Purpose |
|------|----------|-----------|---------|
| Unit | `tests/host/` | Unity | Firmware component testing |
| Integration | `tests/integration/` | pytest | Agent communication |
| E2E (macOS) | `tests/e2e/` | pytest | Full pipeline |
| E2E (Linux) | `tests/e2eLinux/` | pytest | TAP-based simulation |
| Fuzzing | `tests/fuzzing/` | LibFuzzer | Robustness testing |

### Running Tests on macOS

```bash
# Unit tests
cd tests/host
make test

# Integration tests (requires micro-ROS agent)
docker run -d --name uros-agent -p 8888:8888/udp microros/micro-ros-agent:humble
cd tests/integration && pytest -v

# E2E tests (requires dashboard running)
cd tests/e2e
pip install -r requirements.txt
pytest -v

# Renode simulation (macOS uses bundled app)
./Renode.app/Contents/MacOS/renode tests/simulation/renode/stm32h755.resc
```

### Running Tests on Linux

```bash
# Unit tests (same as macOS)
cd tests/host && make test

# Static analysis
make cppcheck
make clang-tidy

# Renode with TAP interface (requires sudo)
sudo apt install renode
cd tests/simulation/scripts
sudo ./run_renode.sh --headless

# E2E Linux tests
cd tests/e2eLinux
sudo $(which python) -m pytest -v --tap
```

### Renode Simulation

**Platform Files:**

| File | Purpose |
|------|---------|
| `stm32h755.repl` | Basic Cortex-M7 simulation |
| `stm32h755_networked.repl` | With Ethernet peripheral |
| `stm32h755_tap.repl` | TAP interface for host networking |

**Running Simulation:**

```bash
# Interactive mode
renode tests/simulation/renode/stm32h755_networked.resc

# Headless with script
renode --disable-xwt --console \
  -e "s@tests/simulation/renode/stm32h755.resc" \
  -e "simulation Start"
```

### Real-time Logging

```bash
# Firmware debug output (UART)
screen /dev/tty.usbserial 115200

# Dashboard logs
tail -f microk3/logs/app.log

# E2E test logs
tail -f tests/e2eLinux/logs/e2e_debug_*.log

# Docker container logs
docker logs -f microk3
docker logs -f uros-agent
```

---

## Network Configuration

### IP Addressing

**Default Configuration:**

| Component | IP Address | Port |
|-----------|------------|------|
| STM32H7 (firmware) | 192.168.0.3 | N/A |
| Host (development) | 192.168.0.1 | N/A |
| micro-ROS Agent | 192.168.0.8 | UDP 8888 |
| Dashboard | 0.0.0.0 | TCP 5050 |

### TAP Interface Setup (Linux)

```bash
# Create TAP interface
sudo ip tuntap add dev tap0 mode tap
sudo ip addr add 192.168.0.1/24 dev tap0
sudo ip link set tap0 up

# Add IP alias for agent
sudo ip addr add 192.168.0.8/24 dev tap0

# Verify
ip addr show tap0
```

### Docker Networking

```yaml
# docker-compose.yml network configuration
services:
  microk3:
    ports:
      - "5050:5050"
    networks:
      - ros_net

  uros-agent:
    ports:
      - "8888:8888/udp"
    networks:
      - ros_net

networks:
  ros_net:
    driver: bridge
```

### ROS 2 Topics Discovery

```bash
# List topics
ros2 topic list

# Expected output
/microk3/node_status
/microk3/system_alerts
/microk3/commands

# Monitor topic
ros2 topic echo /microk3/node_status

# Publish test message
ros2 topic pub --once /microk3/node_status std_msgs/msg/String \
  "{data: '{\"id\": 1, \"status\": \"active\", \"health\": 100}'}"
```

---

## Troubleshooting

### Common Issues

#### Firmware Build Fails

```
Error: micro_ros_stm32cubemx_utils/microros_static_library/libmicroros/libmicroros.a: No such file
```

**Solution:** Generate the micro-ROS library:
```bash
docker run -it --rm -v $(pwd):/project \
  --env MICROROS_LIBRARY_FOLDER=micro_ros_stm32cubemx_utils/microros_static_library \
  microros/micro_ros_static_library_builder:humble
```

#### Ethernet Not Working

**Symptoms:** No link, no packets

**Checklist:**
1. Verify PHY address matches hardware (usually 0)
2. Ensure RMII mode selected in CubeMX
3. Disable D-Cache in `main.c` for DMA stability
4. Verify MPU regions for SRAM buffers

#### Renode Not Found (Linux)

```
Tests skipped: Renode not found
```

**Solution:**
```bash
sudo apt-add-repository ppa:antmicro/renode
sudo apt update && sudo apt install renode
```

#### Dashboard Can't Connect to ROS 2

**Symptoms:** `ros_connected: false`

**Solutions:**
1. Ensure micro-ROS agent is running
2. Check `ROS_DOMAIN_ID` matches
3. Verify UDP port 8888 is open
4. Check firewall rules

#### TAP Permission Denied

```
Error: Permission denied on TAP device
```

**Solution:** Run tests with sudo:
```bash
sudo $(which python) -m pytest tests/e2eLinux/ -v
```

### Debug Logging

**Enable verbose logging:**

```bash
# Dashboard
LOG_LEVEL=DEBUG python app.py

# micro-ROS agent
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888 -v 6

# Firmware (add to code)
#define MICROROS_LOG_LEVEL MICROROS_LOG_LEVEL_DEBUG
```

### Known Limitations

| Issue | Status | Workaround |
|-------|--------|------------|
| Renode.app macOS only | By design | Install Renode on Linux |
| D-Cache must be disabled | Hardware limitation | Required for Ethernet DMA |
| FreeRTOS heap fragmentation | Monitoring | Periodic node restart |
| UDP packet loss in simulation | Expected | Increase timeout values |

### FAQ

**Q: Can I use this with ROS 2 Iron or Jazzy?**

A: Yes, change the Docker image tag from `humble` to `iron` or `jazzy`.

**Q: Does this support CAN bus transport?**

A: Not currently. Only Ethernet (UDP) transport is implemented.

**Q: Can I run multiple STM32 nodes?**

A: Yes, each node needs a unique IP address and agent discovery handles multiple clients.

**Q: How do I add custom ROS 2 message types?**

A: Add packages to `micro_ros_stm32cubemx_utils/microros_static_library/library_generation/extra_packages/`

---

## Development Guide

### Code Style

**C/C++ Firmware:**
- Follow ST HAL coding conventions
- Use `USER_CODE_BEGIN/END` blocks in CubeMX files
- 4-space indentation, no tabs

**Python:**
- PEP 8 compliance
- Type hints required for public functions
- Docstrings for modules and classes

**Commit Messages:**
```
<type>(<scope>): <subject>

[optional body]

[optional footer]
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`

### Adding Features

**New ROS 2 Topic:**
1. Add topic subscription in `ros_interface.py`
2. Update callback handler in `app.py`
3. Add API endpoint if external access needed
4. Document in README

**New Transport (Firmware):**
1. Implement in `extra_sources/microros_transports/`
2. Add to `Makefile` sources
3. Create unit tests in `tests/host/`
4. Update CubeMX configuration docs

### Contributing

1. Fork the repository
2. Create feature branch: `git checkout -b feature/my-feature`
3. Make changes with tests
4. Run CI locally: `make test` (unit), `pytest tests/` (integration)
5. Commit with conventional message
6. Push and create pull request

### CI/CD Pipeline

The CI pipeline (`.github/workflows/ci.yml`) runs on every push/PR:

| Job | Duration | Purpose |
|-----|----------|---------|
| unit-tests | ~2 min | Unity test execution |
| static-analysis | ~1 min | GCC warnings, cppcheck, clang-tidy |
| firmware-build | ~3 min | ARM cross-compilation |
| fuzzing | ~10 min | LibFuzzer quick run |
| renode-simulation | ~15 min | Virtual platform tests |
| integration-tests | ~5 min | Agent communication |
| e2e-tests | ~20 min | Full stack validation |

---

## API Reference

### REST Endpoints

#### System Status
```http
GET /api/system_status
```
Response:
```json
{
  "status": "active",
  "nodes_online": 3,
  "total_nodes": 3,
  "tasks_running": 6,
  "network_latency": 0,
  "timestamp": "2026-02-16T10:30:00",
  "ros_connected": true
}
```

#### Nodes List
```http
GET /api/nodes
```
Response:
```json
[
  {
    "id": 1,
    "name": "Node 1",
    "status": "active",
    "type": "STM32H743VIT6",
    "ram": "1MB",
    "flash": "2MB",
    "cpu": "480MHz",
    "active_tasks": ["Motor Control"],
    "health_score": 95,
    "uptime": "12h 34m",
    "network": "Ethernet"
  }
]
```

#### Update Node
```http
POST /api/update_node
Authorization: Basic <credentials>
Content-Type: application/json

{
  "node_id": 1,
  "status": "standby"
}
```

### ROS 2 Topics

| Topic | Message Type | QoS |
|-------|--------------|-----|
| `/microk3/node_status` | `std_msgs/String` | Reliable |
| `/microk3/system_alerts` | `std_msgs/String` | Reliable |
| `/microk3/commands` | `std_msgs/String` | Reliable |

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `MICROROS_AGENT_IP` | string | 192.168.0.8 | Agent address |
| `MICROROS_AGENT_PORT` | int | 8888 | Agent UDP port |
| `LWIP_DHCP` | bool | false | Enable DHCP |
| `FREERTOS_HEAP_SIZE` | int | 65536 | RTOS heap bytes |


---


### Dependencies

- [micro-ROS](https://micro.ros.org/) - ROS 2 on microcontrollers
- [Micro XRCE-DDS](https://micro-xrce-dds.docs.eprosima.com/) - DDS middleware
- [LwIP](https://savannah.nongnu.org/projects/lwip/) - Lightweight IP stack
- [FreeRTOS](https://www.freertos.org/) - Real-time operating system
- [Renode](https://renode.io/) - IoT simulation framework
- [Flask](https://flask.palletsprojects.com/) - Web framework
