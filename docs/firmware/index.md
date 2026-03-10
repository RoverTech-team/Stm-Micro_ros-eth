---
title: Firmware
nav_order: 2
has_children: true
---

# Firmware
{: .no_toc }

Documentation for the STM32H755 dual-core firmware and the JSN-SR04T demo flow.

---

<div class="section-grid" style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 1rem;">
  <div class="section-card" style="padding: 1.25rem; border: 1px solid #333; border-radius: 8px; background: #161b22;">
    <h4><a href="overview.md">📋 Overview</a></h4>
    <p>Architecture, core split, and shared memory synchronization.</p>
  </div>
  <div class="section-card" style="padding: 1.25rem; border: 1px solid #333; border-radius: 8px; background: #161b22;">
    <h4><a href="cm7.md">🧠 Cortex-M7</a></h4>
    <p>Primary core setup: FreeRTOS, LwIP, and micro-ROS client.</p>
  </div>
  <div class="section-card" style="padding: 1.25rem; border: 1px solid #333; border-radius: 8px; background: #161b22;">
    <h4><a href="cm4.md">⚡ Cortex-M4</a></h4>
    <p>Auxiliary core: High-speed sensor interfacing and IPC.</p>
  </div>
</div>
