#!/bin/bash
set -euo pipefail

WORKSPACE_DIR="${WORKSPACE:-/workspace}"
FIRMWARE_PROJECT_DIR="${FIRMWARE_PROJECT_DIR:-${WORKSPACE_DIR}/microrosWs/Micro_ros_eth/microroseth}"
HOST_WORKSPACE_DIR="${HOST_WORKSPACE_DIR:-}"
MICROROS_LIBRARY_FOLDER="${MICROROS_LIBRARY_FOLDER:-micro_ros_stm32cubemx_utils/microros_static_library}"
MICROROS_BUILDER_TAG="${MICROROS_BUILDER_TAG:-humble}"
BUILD_MICROROS_LIB="${BUILD_MICROROS_LIB:-0}"
BUILD_FIRMWARE="${BUILD_FIRMWARE:-1}"
CM4_MAKE_DIR="${CM4_MAKE_DIR:-${FIRMWARE_PROJECT_DIR}/Makefile/CM4}"
CM7_MAKE_DIR="${CM7_MAKE_DIR:-${FIRMWARE_PROJECT_DIR}/Makefile/CM7}"

host_firmware_project_dir() {
  local relative_path

  case "${FIRMWARE_PROJECT_DIR}" in
    "${WORKSPACE_DIR}"/*)
      relative_path="${FIRMWARE_PROJECT_DIR#"${WORKSPACE_DIR}/"}"
      ;;
    *)
      echo "FIRMWARE_PROJECT_DIR must be under ${WORKSPACE_DIR}: ${FIRMWARE_PROJECT_DIR}" >&2
      exit 1
      ;;
  esac

  printf '%s/%s\n' "${HOST_WORKSPACE_DIR}" "${relative_path}"
}

validate_make_dirs() {
  local make_dir

  for make_dir in "${CM4_MAKE_DIR}" "${CM7_MAKE_DIR}"; do
    if [[ ! -f "${make_dir}/Makefile" ]]; then
      echo "Missing Makefile in firmware build directory: ${make_dir}" >&2
      exit 1
    fi
  done
}

library_archive_path() {
  printf '%s/%s/libmicroros/libmicroros.a\n' "${FIRMWARE_PROJECT_DIR}" "${MICROROS_LIBRARY_FOLDER}"
}

should_build_microros_library() {
  case "${BUILD_MICROROS_LIB}" in
    1|true|TRUE|yes|YES|force|FORCE)
      return 0
      ;;
    auto|AUTO)
      [ ! -f "$(library_archive_path)" ]
      return
      ;;
    *)
      return 1
      ;;
  esac
}

build_microros_library() {
  if ! command -v docker >/dev/null 2>&1; then
    echo "docker CLI not found in firmware-build container" >&2
    exit 1
  fi

  if [ ! -S /var/run/docker.sock ]; then
    echo "/var/run/docker.sock is not mounted; cannot rebuild micro-ROS library" >&2
    exit 1
  fi

  if [ -z "${HOST_WORKSPACE_DIR}" ]; then
    echo "HOST_WORKSPACE_DIR is not set; cannot map firmware project path for nested Docker" >&2
    exit 1
  fi

  HOST_FIRMWARE_PROJECT_DIR="$(host_firmware_project_dir)"

  echo "Rebuilding micro-ROS static library with builder tag: ${MICROROS_BUILDER_TAG}"
  docker run --rm \
    -v "${HOST_FIRMWARE_PROJECT_DIR}:/project" \
    -e MICROROS_LIBRARY_FOLDER="${MICROROS_LIBRARY_FOLDER}" \
    -e MICROROS_ASSUME_CFLAGS_YES=1 \
    "microros/micro_ros_static_library_builder:${MICROROS_BUILDER_TAG}"
}

validate_make_dirs

if should_build_microros_library; then
  build_microros_library
elif [ "${BUILD_MICROROS_LIB}" = "auto" ] || [ "${BUILD_MICROROS_LIB}" = "AUTO" ]; then
  echo "Skipping micro-ROS static library rebuild; existing archive found at $(library_archive_path)"
fi

if [ "${BUILD_FIRMWARE}" = "1" ] || [ "${BUILD_FIRMWARE}" = "true" ]; then
  make -C "${CM4_MAKE_DIR}" -j"$(nproc)"
  make -C "${CM7_MAKE_DIR}" -j"$(nproc)"
fi
