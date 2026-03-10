#!/usr/bin/env bash

set -euo pipefail

WORKSPACE="${WORKSPACE:-/workspace}"
TAP_INTERFACE="${TAP_INTERFACE:-tap0}"
TAP_GATEWAY_CIDR="${TAP_GATEWAY_CIDR:-192.168.50.1/24}"
AGENT_PORT="${AGENT_PORT:-8888}"
RENODE_SCRIPT="${RENODE_SCRIPT:-${WORKSPACE}/microrosWs/tests/e2e/renode/microroseth_docker_tap.resc}"
CM7_ELF="${CM7_ELF:-${WORKSPACE}/microrosWs/Micro_ros_eth/microroseth/Makefile/CM7/build/MicroRosEth_CM7.elf}"
CM4_ELF="${CM4_ELF:-${WORKSPACE}/microrosWs/Micro_ros_eth/microroseth/Makefile/CM4/build/MicroRosEth_CM4.elf}"
AGENT_START_DELAY_SEC="${AGENT_START_DELAY_SEC:-0}"
AGENT_RESTART_AFTER_SEC="${AGENT_RESTART_AFTER_SEC:-0}"
AGENT_RESTART_DOWNTIME_SEC="${AGENT_RESTART_DOWNTIME_SEC:-0}"
TAP_FLAP_AFTER_SEC="${TAP_FLAP_AFTER_SEC:-0}"
TAP_FLAP_DOWNTIME_SEC="${TAP_FLAP_DOWNTIME_SEC:-0}"
RENODE_RESTART_AFTER_SEC="${RENODE_RESTART_AFTER_SEC:-0}"
RENODE_RESTART_DOWNTIME_SEC="${RENODE_RESTART_DOWNTIME_SEC:-0}"
AGENT_PID_FILE="/tmp/micro-ros-agent.pid"
AGENT_BIN="${AGENT_BIN:-/uros_ws/install/micro_ros_agent/lib/micro_ros_agent/micro_ros_agent}"

prepare_tap() {
  if [[ ! -c /dev/net/tun ]]; then
    echo "Missing /dev/net/tun; run this container privileged with /dev/net/tun available." >&2
    exit 1
  fi

  if ! ip link show "${TAP_INTERFACE}" >/dev/null 2>&1; then
    ip tuntap add dev "${TAP_INTERFACE}" mode tap
  fi

  ip addr flush dev "${TAP_INTERFACE}" || true
  ip addr add "${TAP_GATEWAY_CIDR}" dev "${TAP_INTERFACE}"
  ip link set "${TAP_INTERFACE}" up
}

wait_for_firmware() {
  local elf

  for elf in "${CM4_ELF}" "${CM7_ELF}"; do
    if [[ ! -f "${elf}" ]]; then
      echo "Missing firmware artifact: ${elf}" >&2
      echo "Run the firmware-build container successfully before starting renode-e2e." >&2
      exit 1
    fi
  done
}

start_agent() {
  if [[ ! -x "${AGENT_BIN}" ]]; then
    echo "Missing micro-ROS agent binary: ${AGENT_BIN}" >&2
    exit 1
  fi

  export AMENT_TRACE_SETUP_FILES="${AMENT_TRACE_SETUP_FILES:-}"
  export AMENT_PYTHON_EXECUTABLE="${AMENT_PYTHON_EXECUTABLE:-/usr/bin/python3}"
  export AMENT_PREFIX_PATH="${AMENT_PREFIX_PATH:-}"
  export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-}"
  export COLCON_TRACE="${COLCON_TRACE:-}"
  export COLCON_CURRENT_PREFIX="${COLCON_CURRENT_PREFIX:-}"
  export COLCON_PREFIX_PATH="${COLCON_PREFIX_PATH:-}"
  export COLCON_PYTHON_EXECUTABLE="${COLCON_PYTHON_EXECUTABLE:-/usr/bin/python3}"
  export PYTHONPATH="${PYTHONPATH:-}"
  export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}"
  export PATH="${PATH:-/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin}"
  . "/opt/ros/${ROS_DISTRO}/setup.sh"
  . "/uros_ws/install/local_setup.sh"
  ros2 run micro_ros_agent micro_ros_agent udp4 --port "${AGENT_PORT}" -v6 &
  AGENT_PID=$!
  echo "${AGENT_PID}" > "${AGENT_PID_FILE}"
  echo "Started micro-ROS agent pid=${AGENT_PID}"
}

stop_agent() {
  local pid

  if [[ -f "${AGENT_PID_FILE}" ]]; then
    pid="$(cat "${AGENT_PID_FILE}")"
    echo "Stopping micro-ROS agent pid=${pid}"
    kill "${pid}" >/dev/null 2>&1 || true
    wait "${pid}" >/dev/null 2>&1 || true
    rm -f "${AGENT_PID_FILE}"
  fi

  pkill -f "${AGENT_BIN}" >/dev/null 2>&1 || true
  AGENT_PID=
}

schedule_agent_lifecycle() {
  if [[ "${AGENT_START_DELAY_SEC}" != "0" ]]; then
    (
      sleep "${AGENT_START_DELAY_SEC}"
      echo "Starting micro-ROS agent after ${AGENT_START_DELAY_SEC}s delay"
      start_agent
    ) &
  else
    start_agent
  fi

  if [[ "${AGENT_RESTART_AFTER_SEC}" != "0" ]]; then
    (
      sleep "${AGENT_RESTART_AFTER_SEC}"
      echo "Restarting micro-ROS agent after ${AGENT_RESTART_AFTER_SEC}s"
      stop_agent
      sleep "${AGENT_RESTART_DOWNTIME_SEC}"
      start_agent
    ) &
  fi
}

schedule_tap_flap() {
  if [[ "${TAP_FLAP_AFTER_SEC}" == "0" ]]; then
    return
  fi

  (
    sleep "${TAP_FLAP_AFTER_SEC}"
    echo "Bringing ${TAP_INTERFACE} down for ${TAP_FLAP_DOWNTIME_SEC}s"
    ip link set "${TAP_INTERFACE}" down
    sleep "${TAP_FLAP_DOWNTIME_SEC}"
    ip link set "${TAP_INTERFACE}" up
    ip addr flush dev "${TAP_INTERFACE}" || true
    ip addr add "${TAP_GATEWAY_CIDR}" dev "${TAP_INTERFACE}"
  ) &
}

run_renode() {
  local restart_marker
  local restart_delay
  local restart_downtime

  restart_marker="/tmp/renode-restart-requested"
  restart_delay="${RENODE_RESTART_AFTER_SEC}"
  restart_downtime="${RENODE_RESTART_DOWNTIME_SEC}"
  rm -f "${restart_marker}"

  while true; do
    "${RENODE_PATH}" --disable-gui --plain "${RENODE_SCRIPT}" &
    RENODE_PID=$!

    if [[ "${restart_delay}" != "0" ]]; then
      (
        sleep "${restart_delay}"
        if kill -0 "${RENODE_PID}" >/dev/null 2>&1; then
          echo "Restarting Renode after ${restart_delay}s"
          touch "${restart_marker}"
          kill "${RENODE_PID}" >/dev/null 2>&1 || true
        fi
      ) &
      RESTART_TIMER_PID=$!
      restart_delay=0
    else
      RESTART_TIMER_PID=
    fi

    RENODE_RC=0
    wait "${RENODE_PID}" || RENODE_RC=$?
    RENODE_PID=

    if [[ -n "${RESTART_TIMER_PID:-}" ]]; then
      kill "${RESTART_TIMER_PID}" >/dev/null 2>&1 || true
      wait "${RESTART_TIMER_PID}" >/dev/null 2>&1 || true
      RESTART_TIMER_PID=
    fi

    if [[ -f "${restart_marker}" ]]; then
      rm -f "${restart_marker}"
      sleep "${restart_downtime}"
      continue
    fi

    return "${RENODE_RC}"
  done
}

cleanup() {
  if [[ -n "${RENODE_PID:-}" ]]; then
    kill "${RENODE_PID}" >/dev/null 2>&1 || true
    wait "${RENODE_PID}" >/dev/null 2>&1 || true
  fi
  stop_agent
  rm -f "${AGENT_PID_FILE}"
}

trap cleanup EXIT

prepare_tap
wait_for_firmware
schedule_agent_lifecycle
schedule_tap_flap
sleep 2
run_renode
