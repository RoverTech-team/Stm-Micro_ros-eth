#!/usr/bin/env bash
# ==============================================================================
# Caddy TLS & Certificate Management Helper
# Facilitates on-demand TLS cert reset, Root CA export, and connection testing.
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPOSE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

show_help() {
    cat << 'HELP_EOF'
Usage: ./setup_caddy_certs.sh [command]

Commands:
  export-ca      Export Caddy's internal Root CA to ./caddy-root-ca.crt for client trust
  reset-certs    Reset the Caddy certificate cache volume (fixes stale SAN/host mismatches)
  test [IP/HOST] Test HTTPS and TLS handshake against target (default: localhost)
  status         Show Caddy container status and recent certificate logs
  help           Show this help message

Examples:
  ./setup_caddy_certs.sh export-ca
  ./setup_caddy_certs.sh test 10.42.0.1
  ./setup_caddy_certs.sh reset-certs
HELP_EOF
}

COMMAND="${1:-help}"

case "${COMMAND}" in
    export-ca)
        echo "==> Exporting Caddy Root CA certificate..."
        CONTAINER_ID=$(docker ps -q -f "name=caddy" | head -n 1)
        if [[ -z "${CONTAINER_ID}" ]]; then
            echo "Error: Caddy container is not running."
            exit 1
        fi
        
        OUTPUT_FILE="${COMPOSE_DIR}/caddy-root-ca.crt"
        if docker exec "${CONTAINER_ID}" test -f /data/caddy/pki/authorities/local/root.crt; then
            docker cp "${CONTAINER_ID}:/data/caddy/pki/authorities/local/root.crt" "${OUTPUT_FILE}"
            echo "Successfully exported Root CA to: ${OUTPUT_FILE}"
            echo ""
            echo "To trust on macOS:"
            echo "  sudo security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain \"${OUTPUT_FILE}\""
            echo "To trust on Linux (Ubuntu/Debian):"
            echo "  sudo cp \"${OUTPUT_FILE}\" /usr/local/share/ca-certificates/caddy-root.crt && sudo update-ca-certificates"
        else
            echo "Error: Root CA not yet generated in Caddy container (/data/caddy/pki/authorities/local/root.crt)."
            echo "Trigger a request first (e.g. curl -k https://localhost:5050) then re-run this command."
            exit 1
        fi
        ;;

    reset-certs)
        echo "==> Resetting Caddy certificates volume..."
        # Auto-confirm in non-interactive or when second arg is -y/--yes
        AUTO_YES="0"
        if [[ "${2:-}" == "-y" || "${2:-}" == "--yes" ]]; then AUTO_YES="1"; fi
        if [[ "$AUTO_YES" == "0" ]]; then
            read -p "This will remove cached Caddy TLS certificates. Continue? [y/N] " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then echo "Aborted."; exit 0; fi
        fi
        cd "${COMPOSE_DIR}"
        # Resolve actual compose file for current STACK (hil-host vs jetson etc.)
        if [[ -f "${COMPOSE_DIR}/scripts/resolve-compose.sh" ]]; then
            RESOLVED_COMPOSE="$("${COMPOSE_DIR}/scripts/resolve-compose.sh" 2>/dev/null || echo "${COMPOSE_DIR}/docker-compose.jetson.yml")"
        else
            RESOLVED_COMPOSE="${COMPOSE_DIR}/docker-compose.jetson.yml"
        fi
        echo "Stopping Caddy container (compose: ${RESOLVED_COMPOSE})..."
        docker compose -f "${RESOLVED_COMPOSE}" stop caddy 2>/dev/null || docker compose stop caddy 2>/dev/null || true
        echo "Removing caddy_data volumes (all STACK variants)..."
        for vol in jetson-orin-nx_caddy_data jetson-orin-nx_caddy_config caddy_data caddy_config; do
            docker volume rm "${vol}" 2>/dev/null || true
        done
        # Also try STACK-specific volume names
        for vol in microk3_hil_host_caddy_data caddy_data; do docker volume rm "${vol}" 2>/dev/null || true; done
        echo "Restarting Caddy..."
        docker compose -f "${RESOLVED_COMPOSE}" up -d caddy 2>/dev/null || docker compose up -d caddy 2>/dev/null || true
        echo "Done. Fresh internal CA and certificates will be generated on demand (any IP via :5050 on_demand TLS)."
        ;;

    test)
        # If no host given, try to auto-detect current LAN IP, fallback to localhost
        if [[ -z "${2:-}" ]]; then
            DETECTED_IP="$(hostname -I 2>/dev/null | awk '{print $1}' || ip route get 1.1.1.1 2>/dev/null | awk '/src/ {print $7; exit}' || echo "localhost")"
            # Prefer a non-loopback, non-docker IP; if detection yields empty, use localhost
            if [[ -z "$DETECTED_IP" || "$DETECTED_IP" == "127.0.0.1" ]]; then DETECTED_IP="localhost"; fi
            TARGET_HOST="$DETECTED_IP"
        else
            TARGET_HOST="${2}"
        fi
        PORT="${DASHBOARD_PORT:-5050}"
        HTTP_PORT="${DASHBOARD_HTTP_PORT:-8080}"
        echo "==> Testing TLS handshake to https://${TARGET_HOST}:${PORT} (and http fallback :${HTTP_PORT})..."
        
        if command -v openssl >/dev/null 2>&1; then
            echo "--- OpenSSL Handshake Check (HTTPS :${PORT}) ---"
            echo | openssl s_client -connect "${TARGET_HOST}:${PORT}" -servername "${TARGET_HOST}" 2>&1 | grep -E "CN =|Verify return code|Protocol|Cipher" || true
            echo ""
        fi
        
        echo "--- HTTPS Health Check (insecure/self-signed, any IP) ---"
        curl -vik "https://${TARGET_HOST}:${PORT}/health" 2>&1 | head -n 30 || true
        echo ""
        echo "--- HTTP Fallback Check (plain http :${HTTP_PORT}) ---"
        curl -vik "http://${TARGET_HOST}:${HTTP_PORT}/health" 2>&1 | head -n 30 || true
        ;;

    status)
        echo "==> Checking Caddy status..."
        CONTAINER_ID=$(docker ps -q -f "name=caddy" | head -n 1)
        if [[ -n "${CONTAINER_ID}" ]]; then
            docker ps -f "name=caddy"
            echo ""
            echo "Recent Caddy TLS logs:"
            docker logs "${CONTAINER_ID}" --tail 30
        else
            echo "Caddy is not running."
        fi
        ;;

    help|*)
        show_help
        ;;
esac
