#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"

read_env_value() {
  key="$1"
  file="$2"
  if [ ! -f "$file" ]; then
    return 1
  fi
  awk -F= -v k="$key" '
    $1 == k {
      $1="";
      sub(/^=/,"");
      gsub(/^[ \t]+|[ \t]+$/, "", $0);
      gsub(/^"|"$/, "", $0);
      print $0;
      exit
    }
  ' "$file"
}

DASHBOARD_PORT="$(read_env_value DASHBOARD_PORT "$ENV_FILE" || true)"
RENODE_NODE_ID="$(read_env_value RENODE_NODE_ID "$ENV_FILE" || true)"

DASHBOARD_PORT="${DASHBOARD_PORT:-5050}"
RENODE_NODE_ID="${RENODE_NODE_ID:-755}"

HEALTH_URL="http://127.0.0.1:${DASHBOARD_PORT}/health"
NODES_URL="http://127.0.0.1:${DASHBOARD_PORT}/api/nodes"

curl -fsS "$HEALTH_URL" >/dev/null

if ! curl -fsS "$NODES_URL" | grep -q "\"id\":${RENODE_NODE_ID}"; then
  echo "Node ${RENODE_NODE_ID} not found in /api/nodes."
  exit 1
fi

echo "Healthcheck OK. Node ${RENODE_NODE_ID} present."
