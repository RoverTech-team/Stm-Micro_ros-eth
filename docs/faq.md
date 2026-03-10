---
title: FAQ
nav_order: 13
---

# Frequently Asked Questions (FAQ)

<details>
<summary>Why use Ethernet instead of Serial for micro-ROS?</summary>
<div>
  <p>Ethernet offers significantly higher bandwidth, lower latency, and better scalability for complex ROS 2 systems. It also allows the microcontroller to reside on a standard IP network.</p>
</div>
</details>

<details>
<summary>Can I use both cores for micro-ROS?</summary>
<div>
  <p>In this implementation, only the <strong>CM7</strong> core runs the micro-ROS client and the LwIP stack. The <strong>CM4</strong> core is reserved for time-critical auxiliary tasks (like high-speed sensor reading) that communicate with the CM7 via shared memory and HSEM.</p>
</div>
</details>

<details>
<summary>Does this support ROS 2 Humble?</summary>
<div>
  <p>Yes, this project is fully compatible with <strong>ROS 2 Humble</strong>. Ensure your host agent and microk3 environment are also running Humble.</p>
</div>
</details>

<details>
<summary>How do I change the IP address?</summary>
<div>
  <p>The static IP is currently configured in the CM7 source code within the LwIP initialization. See <a href="firmware/cm7.html">CM7 Configuration</a> for details.</p>
</div>
</details>

<details>
<summary>Why is my build failing on macOS?</summary>
<div>
  <p>Building the <code>libmicroros.a</code> library currently requires a Linux-based ROS 2 environment. We recommend using the provided <strong>Docker</strong> build method if you are on macOS or Windows.</p>
</div>
</details>
