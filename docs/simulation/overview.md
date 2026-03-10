---
title: Simulation Overview
parent: Simulation
nav_order: 1
---

# Simulation Overview
{: .no_toc }

This project uses **Renode** for functional simulation of the STM32H755 firmware and the JSN-SR04T ultrasonic sensor.

---

## Renode in Action
{: .fs-6 }

Simulation allows for rapid testing of the micro-ROS state machine without physical hardware.

<div class="screenshot-placeholder">
  <span>📷 Renode Simulation Screenshot — Coming Soon</span>
</div>
*Above: Renode simulating the CM7 core with Ethernet peripheral activity.*

---

## Simulated Components

1.  **STM32H755**: Functional model including Cortex-M7 and Cortex-M4.
2.  **Ethernet (MAC)**: TAP-based networking bridge to the host.
3.  **JSN-SR04T**: Custom C# sensor model providing distance data over UART/GPIO.

---

## Running a Simulation

To start the simulation with the TAP interface:

```bash
# Terminal 1: Setup TAP
sudo ip tuntap add dev tap0 mode tap
sudo ip link set dev tap0 up

# Terminal 2: Run Renode
renode simulation/stm32h755.resc
```

---

## Automated Test Harness

The simulation is integrated with a Python-based test runner that verifies:
*   Successful Ethernet DHCP/Static IP assignment.
*   micro-ROS agent discovery.
*   Topic publishing frequency.
