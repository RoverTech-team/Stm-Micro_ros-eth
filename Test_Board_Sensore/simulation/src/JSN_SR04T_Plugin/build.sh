#!/bin/bash
# Build script for JSN-SR04T Renode plugin.

set -euo pipefail

PLUGIN_NAME="JSN_SR04T_Plugin"
PROJECT_FILE="${PLUGIN_NAME}.csproj"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== JSN-SR04T Renode Plugin Build ===${NC}"

if ! command -v dotnet >/dev/null 2>&1; then
  echo -e "${RED}dotnet SDK not found${NC}"
  exit 1
fi

RENODE_BIN_DIR="${1:-${RENODE_BIN_DIR:-}}"
if [ -z "${RENODE_BIN_DIR}" ]; then
  if [ -d "../../../../renode/output/bin/Release" ]; then
    RENODE_BIN_DIR="../../../../renode/output/bin/Release"
  elif [ -d "${HOME}/.net/renode/ThLhnt0ejjXPsWUsqImYupJxhG6fITM=" ]; then
    RENODE_BIN_DIR="${HOME}/.net/renode/ThLhnt0ejjXPsWUsqImYupJxhG6fITM="
  fi
fi

if [ -z "${RENODE_BIN_DIR}" ] || [ ! -d "${RENODE_BIN_DIR}" ]; then
  echo -e "${YELLOW}Renode binary directory not found.${NC}"
  echo "Pass it explicitly: ./build.sh /path/to/renode/bin-or-output-dir"
  exit 1
fi

echo "Using Renode binaries from: ${RENODE_BIN_DIR}"
DOTNET_CLI_HOME="${DOTNET_CLI_HOME:-/tmp/dotnethome}"
mkdir -p "${DOTNET_CLI_HOME}"

dotnet build "${PROJECT_FILE}" -c Release \
  /p:RenodeBinDir="${RENODE_BIN_DIR}" \
  --no-restore \
  --nologo

OUTPUT_DLL="bin/Release/net8.0/${PLUGIN_NAME}.dll"
if [ ! -f "${OUTPUT_DLL}" ]; then
  echo -e "${RED}Build completed but ${OUTPUT_DLL} was not produced.${NC}"
  exit 1
fi

echo -e "${GREEN}Build successful${NC}"
echo "Output: ${OUTPUT_DLL}"
