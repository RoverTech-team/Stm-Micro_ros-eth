#!/bin/bash
# Save as build_microros.sh and run with: bash build_microros.sh
set -e
# Install micromamba if not present
if ! command -v micromamba &> /dev/null; then
    echo "Installing micromamba..."
    brew install micromamba
    micromamba shell init -s zsh
    source ~/.zshrc
fi
# Create and activate ROS 2 environment
echo "Creating ROS 2 environment..."
micromamba create -n ros2 python=3.10 -c conda-forge -y
micromamba activate ros2
# Install ROS 2 Humble
echo "Installing ROS 2 Humble..."
micromamba install -c conda-forge ros-humble-desktop -y
# Install build tools
echo "Installing colcon..."
pip install colcon-common-extensions lark
# Navigate to project
cd /Users/giuliomastromartino/Documents/Polispace/DEV/MICRO_ROS_ETH-main/Micro_ros_eth/microroseth
# Build the library
echo "Building micro-ROS library..."
cd micro_ros_stm32cubemx_utils/microros_static_library/library_generation
colcon build
echo "Done! Library built successfully."
