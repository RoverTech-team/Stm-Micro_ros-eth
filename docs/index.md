---
title: Home
nav_order: 1
---

[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen)](https://github.com/RoverTech-team/Stm-Micro_ros-eth/actions)
[![ROS 2](https://img.shields.io/badge/ROS_2-Humble-blue)](https://docs.ros.org/en/humble/index.html)
[![License](https://img.shields.io/badge/License-MIT-yellow)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-STM32H7-orange)](https://www.st.com/en/microcontrollers-microprocessors/stm32h7-series.html)

# MICRO_ROS_ETH
{: .fs-9 }

Ethernet-based micro-ROS on STM32H7 dual-core microcontrollers.
{: .fs-6 .fw-300 }

[Get Started](production/quickstart.html){: .btn .btn-primary .fs-5 .mb-4 .mb-md-0 .mr-2 } [View on GitHub](https://github.com/RoverTech-team/Stm-Micro_ros-eth){: .btn .fs-5 .mb-4 .mb-md-0 }

---

## Key Features

<div class="feature-grid" style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 1rem; margin-top: 2rem;">
  <div class="feature-card" style="padding: 1.5rem; border: 1px solid #444; border-radius: 8px; background: #222;">
    <h3>⚡ High Performance</h3>
    <p>Cortex-M7 (480 MHz) + Cortex-M4 (240 MHz) with FreeRTOS and LwIP for low-latency communication.</p>
  </div>
  <div class="feature-card" style="padding: 1.5rem; border: 1px solid #444; border-radius: 8px; background: #222;">
    <h3>📡 micro-ROS Ready</h3>
    <p>Optimized XRCE-DDS over Ethernet transport layer for seamless ROS 2 integration.</p>
  </div>
  <div class="feature-card" style="padding: 1.5rem; border: 1px solid #444; border-radius: 8px; background: #222;">
    <h3>📊 Live Dashboard</h3>
    <p>Real-time monitoring via <b>microk3</b> Flask-based REST API and web UI.</p>
  </div>
  <div class="feature-card" style="padding: 1.5rem; border: 1px solid #444; border-radius: 8px; background: #222;">
    <h3>🛠️ Simulation First</h3>
    <p>Test firmware in Renode with custom sensor models before hardware deployment.</p>
  </div>
</div>

---

## Documentation Sections

<div class="section-grid" style="display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 1.5rem; margin-top: 2rem;">
  <div class="section-card" style="padding: 1.25rem; border: 1px solid #333; border-radius: 8px;">
    <h4><a href="{{ '/firmware/overview.html' | relative_url }}">💾 Firmware</a></h4>
    <p>STM32H7 dual-core build system, CM7/CM4 split, and HSEM synchronization.</p>
  </div>
  <div class="section-card" style="padding: 1.25rem; border: 1px solid #333; border-radius: 8px;">
    <h4><a href="{{ '/microros/overview.html' | relative_url }}">🤖 micro-ROS</a></h4>
    <p>Transport layer, RMW configuration, and ROS 2 topic mapping.</p>
  </div>
  <div class="section-card" style="padding: 1.25rem; border: 1px solid #333; border-radius: 8px;">
    <h4><a href="{{ '/microk3/overview.html' | relative_url }}">🕸️ microk3</a></h4>
    <p>Flask dashboard setup, REST API reference, and rclpy bridge.</p>
  </div>
  <div class="section-card" style="padding: 1.25rem; border: 1px solid #333; border-radius: 8px;">
    <h4><a href="{{ '/simulation/overview.html' | relative_url }}">🔄 Simulation</a></h4>
    <p>Renode scripts, sensor modeling, and automated test runner.</p>
  </div>
  <div class="section-card" style="padding: 1.25rem; border: 1px solid #333; border-radius: 8px;">
    <h4><a href="{{ '/production/overview.html' | relative_url }}">🚀 Production</a></h4>
    <p>Jetson Orin NX deployment and Hardware-in-the-Loop workflows.</p>
  </div>
  <div class="section-card" style="padding: 1.25rem; border: 1px solid #333; border-radius: 8px;">
    <h4><a href="{{ '/hardware/reference.html' | relative_url }}">🔌 Hardware</a></h4>
    <p>Wiring diagrams, pinouts, and supported development boards.</p>
  </div>
</div>

---

<div style="display: flex; gap: 1rem; margin-top: 2rem;">
  <a href="{{ '/glossary.html' | relative_url }}" class="btn btn-outline">Glossary</a>
  <a href="{{ '/faq.html' | relative_url }}" class="btn btn-outline">FAQ</a>
  <a href="{{ '/troubleshooting.html' | relative_url }}" class="btn btn-outline">Troubleshooting</a>
</div>
