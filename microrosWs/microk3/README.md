# MicroK3 🚀

![Version](https://img.shields.io/badge/version-0.2.1-blue)
![Python](https://img.shields.io/badge/python-3.9+-green)
![ROS 2](https://img.shields.io/badge/ROS%202-Humble%2Fjazzy-blueviolet)
![License](https://img.shields.io/badge/license-Apache%202.0-orange)

Flask + rclpy dashboard for monitoring micro-ROS nodes and exposing a REST API. Full documentation is hosted on GitHub Pages.

The dashboard includes a dedicated `ROS 2` page for browsing discovered nodes/topics and watching live topic values with short rolling history.

The `Images` page shows one tile per feed from three sources: ROS
(`sensor_msgs/Image`, including `mono8`, or `CompressedImage` topics),
raw JPEG over TCP (`tcp://host:port`) or MJPEG over HTTP
(`http://host:port/path`), and raw mono8 over UDP (`udp://host:port`, VFR0
row-block framing, e.g. from an STM32+W5500 video stream). All three serve the
same tile snapshot shape (`jpeg_base64` + `mime`), so the frontend polls them
uniformly.

## Documentation

- https://rovertech-team.github.io/Stm-Micro_ros-eth/microk3/overview.html

## Quick Links

- https://rovertech-team.github.io/Stm-Micro_ros-eth/microk3/api.html
- https://rovertech-team.github.io/Stm-Micro_ros-eth/microros/host.html
