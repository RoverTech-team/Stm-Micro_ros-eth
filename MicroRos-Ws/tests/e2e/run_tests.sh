#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

ENABLE_TAP=false
PYTEST_EXTRA_ARGS=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --tap)
            ENABLE_TAP=true
            shift
            ;;
        --)
            shift
            PYTEST_EXTRA_ARGS="$@"
            break
            ;;
        *)
            PYTEST_EXTRA_ARGS="$PYTEST_EXTRA_ARGS $1"
            shift
            ;;
    esac
done

RUN_MODE="${RUN_MODE:-auto}"

check_docker_network() {
    local network_name="$1"
    if docker network inspect "$network_name" >/dev/null 2>&1; then
        return 0
    fi
    return 1
}

check_microk3_running() {
    if docker ps --format '{{.Names}}' | grep -q '^microk3$'; then
        return 0
    fi
    return 1
}

check_uros_running() {
    if docker ps --format '{{.Names}}' | grep -q '^uros-agent$'; then
        return 0
    fi
    return 1
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        return 1
    fi
    return 0
}

create_external_network() {
    local network_name="microk3_network"
    if ! check_docker_network "$network_name"; then
        echo "Creating external network: $network_name"
        docker network create "$network_name"
    fi
}

connect_to_network() {
    local container_name="$1"
    local network_name="$2"
    
    if docker ps --format '{{.Names}}' | grep -q "^${container_name}$"; then
        if ! docker network inspect "$network_name" 2>/dev/null | grep -q "\"Name\": \"${container_name}\""; then
            echo "Connecting $container_name to $network_name"
            docker network connect "$network_name" "$container_name" 2>/dev/null || true
        fi
    fi
}

run_with_existing_stack() {
    echo "Running tests against existing microk3 stack..."
    
    NETWORK_NAME="microk3_network"
    create_external_network
    
    connect_to_network "microk3" "$NETWORK_NAME"
    connect_to_network "uros-agent" "$NETWORK_NAME"
    
    if [[ "$ENABLE_TAP" == "true" ]]; then
        echo "WARNING: TAP tests require privileged container with host networking."
        echo "Running TAP tests is not supported in 'attached' mode."
        echo "Use RUN_MODE=standalone with --tap for TAP tests."
    fi
    
    PYTEST_ARGS=""
    if [[ -n "$PYTEST_EXTRA_ARGS" ]]; then
        PYTEST_ARGS="-- $PYTEST_EXTRA_ARGS"
    fi
    
    docker compose -f docker-compose.e2e.yml run --rm \
        -e DASHBOARD_URL=http://microk3:5050 \
        -e AGENT_HOST=uros-agent \
        -e AGENT_PORT=8888 \
        -e RUNNING_IN_DOCKER=1 \
        test-runner $PYTEST_ARGS
}

run_standalone() {
    echo "Starting test stack and running tests..."
    
    if [[ "$ENABLE_TAP" == "true" ]]; then
        echo ""
        echo "=========================================="
        echo "Running with TAP tests enabled"
        echo "=========================================="
        echo ""
        echo "Requirements for TAP tests:"
        echo "  - Privileged Docker container (handled automatically)"
        echo "  - Renode.app installed at project root"
        echo "  - STM32 firmware built"
        echo ""
        echo "Network topology:"
        echo "  Docker test-runner (privileged)"
        echo "      |"
        echo "      |-- tap0 (192.168.0.1)"
        echo "      |       |"
        echo "      |       |-- IP alias: 192.168.0.8 (agent)"
        echo "      |       |"
        echo "      |       |-- Renode simulation"
        echo "      |           |-- STM32 (192.168.0.3)"
        echo "      |               |"
        echo "      |               |-- XRCE-DDS UDP -> Agent:8888"
        echo "      |"
        echo "      |-- microk3 network -> Dashboard tests"
        echo ""
        
        echo "Note: TAP tests run in a privileged container with host networking."
        echo ""
        
        PYTEST_ARGS=""
        if [[ -n "$PYTEST_EXTRA_ARGS" ]]; then
            PYTEST_ARGS="-- $PYTEST_EXTRA_ARGS"
        fi
        
        docker compose -f docker-compose.e2e.yml --profile tap up --build --abort-on-container-exit test-runner-tap $PYTEST_ARGS
        
        docker compose -f docker-compose.e2e.yml down -v --remove-orphans
    else
        PYTEST_ARGS=""
        if [[ -n "$PYTEST_EXTRA_ARGS" ]]; then
            PYTEST_ARGS="-- $PYTEST_EXTRA_ARGS"
        fi
        
        docker compose -f docker-compose.e2e.yml --profile test up --build --abort-on-container-exit test-runner $PYTEST_ARGS
        
        docker compose -f docker-compose.e2e.yml down -v --remove-orphans
    fi
}

run_attached() {
    echo "Running test-runner attached to existing compose project..."
    
    PROJECT_NAME="${COMPOSE_PROJECT_NAME:-microk3}"
    NETWORK_NAME="${PROJECT_NAME}_default"
    
    if ! check_microk3_running; then
        echo "Error: microk3 container not found. Start the microk3 stack first."
        exit 1
    fi
    
    if ! check_uros_running; then
        echo "Warning: uros-agent container not found. Some tests may fail."
    fi
    
    create_external_network
    connect_to_network "microk3" "$NETWORK_NAME"
    connect_to_network "uros-agent" "$NETWORK_NAME"
    
    if [[ "$ENABLE_TAP" == "true" ]]; then
        echo "WARNING: TAP tests require privileged container with host networking."
        echo "Running TAP tests is not supported in 'attached' mode."
        echo "Use RUN_MODE=standalone with --tap for TAP tests."
    fi
    
    PYTEST_ARGS=""
    if [[ -n "$PYTEST_EXTRA_ARGS" ]]; then
        PYTEST_ARGS="-- $PYTEST_EXTRA_ARGS"
    fi
    
    docker compose -f docker-compose.e2e.yml run --rm \
        -e DASHBOARD_URL=http://microk3:5050 \
        -e AGENT_HOST=uros-agent \
        -e AGENT_PORT=8888 \
        -e RUNNING_IN_DOCKER=1 \
        test-runner $PYTEST_ARGS
}

show_usage() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS] [-- PYTEST_ARGS]

E2E Test Runner for MicroK3 with optional TAP-based Renode simulation.

Options:
    --tap           Enable TAP-based Renode simulation tests (requires root/sudo)
    --              Pass remaining arguments to pytest

Environment Variables:
    RUN_MODE        Test execution mode (default: auto)
                    - standalone: Start fresh Docker stack and run tests
                    - attached: Attach to existing microk3 stack
                    - auto: Detect existing stack or start new one

Run Modes:
    standalone      Starts microk3, uros-agent, and test-runner containers
                    Tears down containers after tests complete
    
    attached        Connects test-runner to running microk3 stack
                    Does not tear down containers

TAP Tests (--tap):
    Requires:
        - Privileged Docker container (handled automatically)
        - Renode.app at project root
        - STM32 firmware built at Micro_ros_eth/microroseth/Makefile/CM7/build/

    Network Topology:
        Docker test-runner (privileged, host network)
            |
            |-- tap0 (192.168.0.1)
            |       |-- IP alias: 192.168.0.8 (agent)
            |       |-- Renode: STM32 (192.168.0.3) -> XRCE-DDS

Examples:
    # Run standard E2E tests (no TAP)
    ./run_tests.sh
    
    # Run with specific pytest args
    ./run_tests.sh -- -v -k "dashboard"
    
    # Run with TAP tests
    ./run_tests.sh --tap
    
    # Run TAP tests with pytest args
    ./run_tests.sh --tap -- -v -k "xrcedds"

EOF
}

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    show_usage
    exit 0
fi

case "$RUN_MODE" in
    standalone)
        run_standalone
        ;;
    attached)
        run_attached
        ;;
    auto|*)
        if check_microk3_running; then
            run_attached
        else
            run_standalone
        fi
        ;;
esac