#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
E2E_DIR="${ROOT_DIR}/microrosWs/tests/e2e"
COMPOSE_FILE="docker-compose.renode-e2e.yml"
API_URL="${API_URL:-http://127.0.0.1:5050}"
NODE_ID="${NODE_ID:-755}"

cleanup() {
  docker compose -f "${COMPOSE_FILE}" down >/dev/null 2>&1 || true
}

wait_for_json_match() {
  local path="$1"
  local pattern="$2"
  local timeout="${3:-120}"
  local deadline=$((SECONDS + timeout))
  local body

  while (( SECONDS < deadline )); do
    body="$(curl -fsS "${API_URL}${path}" 2>/dev/null || true)"
    if [[ "${body}" == *"${pattern}"* ]]; then
      return 0
    fi
    sleep 2
  done

  echo "Timed out waiting for ${path} to contain ${pattern}" >&2
  return 1
}

wait_for_json_absent() {
  local path="$1"
  local pattern="$2"
  local timeout="${3:-120}"
  local deadline=$((SECONDS + timeout))
  local body

  while (( SECONDS < deadline )); do
    body="$(curl -fsS "${API_URL}${path}" 2>/dev/null || true)"
    if [[ "${body}" != *"${pattern}"* ]]; then
      return 0
    fi
    sleep 2
  done

  echo "Timed out waiting for ${path} to drop ${pattern}" >&2
  return 1
}

run_compose() {
  local scenario="$1"
  shift

  cleanup
  sleep 2
  echo "=== ${scenario} ==="
  if ! env "$@" docker compose -f "${COMPOSE_FILE}" up -d --build; then
    sleep 3
    env "$@" docker compose -f "${COMPOSE_FILE}" up -d --build
  fi
}

assert_logs() {
  local service="$1"
  local pattern="$2"
  local timeout="${3:-120}"
  local deadline=$((SECONDS + timeout))
  local logs

  while (( SECONDS < deadline )); do
    logs="$(docker compose -f "${COMPOSE_FILE}" logs --tail=400 "${service}" 2>/dev/null || true)"
    if [[ "${logs}" == *"${pattern}"* ]]; then
      return 0
    fi
    sleep 2
  done

  echo "Timed out waiting for ${service} logs to contain ${pattern}" >&2
  return 1
}

trap cleanup EXIT

cd "${E2E_DIR}"

run_compose "agent-delay" AGENT_START_DELAY_SEC=10
wait_for_json_match "/api/nodes" "\"id\":${NODE_ID}" 180
wait_for_json_match "/api/nodes/${NODE_ID}/logs" "RAW_HEARTBEAT" 120

run_compose "agent-restart" AGENT_RESTART_AFTER_SEC=60 AGENT_RESTART_DOWNTIME_SEC=8
wait_for_json_match "/api/nodes" "\"id\":${NODE_ID}" 180
wait_for_json_match "/api/failures" "\"node_id\":${NODE_ID}" 180
wait_for_json_match "/api/nodes/${NODE_ID}/logs" "RAW_HEARTBEAT" 180

run_compose "tap-flap" TAP_FLAP_AFTER_SEC=60 TAP_FLAP_DOWNTIME_SEC=8
wait_for_json_match "/api/nodes" "\"id\":${NODE_ID}" 180
wait_for_json_match "/api/failures" "\"node_id\":${NODE_ID}" 180
wait_for_json_match "/api/nodes/${NODE_ID}/logs" "RAW_HEARTBEAT" 180

run_compose "renode-restart" RENODE_RESTART_AFTER_SEC=60 RENODE_RESTART_DOWNTIME_SEC=6
wait_for_json_match "/api/nodes" "\"id\":${NODE_ID}" 180
wait_for_json_match "/api/failures" "\"node_id\":${NODE_ID}" 180
wait_for_json_match "/api/nodes/${NODE_ID}/logs" "RAW_HEARTBEAT" 180

run_compose "phy-down" RENODE_SCRIPT=/workspace/microrosWs/tests/e2e/renode/microroseth_phy_down.resc
wait_for_json_absent "/api/nodes" "\"id\":${NODE_ID}" 60

run_compose "phy-badid" RENODE_SCRIPT=/workspace/microrosWs/tests/e2e/renode/microroseth_phy_badid.resc
wait_for_json_absent "/api/nodes" "\"id\":${NODE_ID}" 60

run_compose "link-flap" RENODE_SCRIPT=/workspace/microrosWs/tests/e2e/renode/microroseth_link_flap.resc TAP_FLAP_AFTER_SEC=60 TAP_FLAP_DOWNTIME_SEC=10
wait_for_json_match "/api/nodes" "\"id\":${NODE_ID}" 180
wait_for_json_match "/api/failures" "\"node_id\":${NODE_ID}" 180

echo "RESULT fault_matrix=1"
