#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
DOCKER_DIR="${SCRIPT_DIR}/docker"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_help() {
    cat << EOF
Docker-based Testing Environment for micro-ROS ETH

Usage: $0 [OPTIONS] [COMMAND]

Commands:
    all             Run all tests (default)
    unit            Run unit tests only
    static          Run static analysis only
    firmware        Build firmware only
    integration     Run integration tests only
    coverage        Generate coverage report
    shell           Open shell in test container
    clean           Clean up Docker resources
    build           Build Docker images only

Options:
    -h, --help      Show this help message
    -v, --verbose   Enable verbose output
    --no-cache      Build Docker images without cache
    --ci            Run in CI mode

Examples:
    $0 all          Run all tests
    $0 unit         Run unit tests only
    $0 shell        Open interactive shell

EOF
}

check_docker() {
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed. Please install Docker first."
        exit 1
    fi

    if ! docker info &> /dev/null; then
        log_error "Docker daemon is not running. Please start Docker."
        exit 1
    fi

    if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
        log_error "Docker Compose is not installed. Please install Docker Compose."
        exit 1
    fi
}

docker_compose_cmd() {
    if docker compose version &> /dev/null; then
        docker compose "$@"
    else
        docker-compose "$@"
    fi
}

build_images() {
    local no_cache=""
    if [ "$NO_CACHE" = "true" ]; then
        no_cache="--no-cache"
    fi

    log_info "Building Docker images..."
    docker_compose_cmd -f "${DOCKER_DIR}/docker-compose.yml" build $no_cache
    log_success "Docker images built successfully"
}

run_unit_tests() {
    log_info "Running unit tests..."
    docker_compose_cmd -f "${DOCKER_DIR}/docker-compose.yml" up unit-tests --exit-code-from unit-tests
    log_success "Unit tests completed"
}

run_static_analysis() {
    log_info "Running static analysis..."
    docker_compose_cmd -f "${DOCKER_DIR}/docker-compose.yml" up static-analysis --exit-code-from static-analysis || true
    log_success "Static analysis completed"
}

run_firmware_build() {
    log_info "Building firmware..."
    docker_compose_cmd -f "${DOCKER_DIR}/docker-compose.yml" up firmware-build --exit-code-from firmware-build
    log_success "Firmware build completed"
}

run_integration_tests() {
    log_info "Running integration tests..."
    docker_compose_cmd -f "${DOCKER_DIR}/docker-compose.yml" up micro-ros-agent -d
    sleep 5
    docker_compose_cmd -f "${DOCKER_DIR}/docker-compose.yml" up integration-tests --exit-code-from integration-tests || true
    docker_compose_cmd -f "${DOCKER_DIR}/docker-compose.yml" stop micro-ros-agent
    log_success "Integration tests completed"
}

run_coverage() {
    log_info "Generating coverage report..."
    docker_compose_cmd -f "${DOCKER_DIR}/docker-compose.yml" up coverage-report
    log_success "Coverage report generated"
}

run_all_tests() {
    log_info "Running all tests..."
    
    run_unit_tests
    run_static_analysis
    run_firmware_build
    run_integration_tests
    run_coverage
    
    log_success "All tests completed!"
}

open_shell() {
    log_info "Opening shell in test container..."
    docker_compose_cmd -f "${DOCKER_DIR}/docker-compose.yml" run --rm test-env bash
}

clean_up() {
    log_info "Cleaning up Docker resources..."
    docker_compose_cmd -f "${DOCKER_DIR}/docker-compose.yml" down -v --remove-orphans
    docker system prune -f
    log_success "Cleanup completed"
}

VERBOSE="false"
NO_CACHE="false"
CI_MODE="false"

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -v|--verbose)
            VERBOSE="true"
            shift
            ;;
        --no-cache)
            NO_CACHE="true"
            shift
            ;;
        --ci)
            CI_MODE="true"
            shift
            ;;
        all|unit|static|firmware|integration|coverage|shell|clean|build)
            COMMAND="$1"
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

check_docker

COMMAND="${COMMAND:-all}"

if [ "$CI_MODE" = "true" ]; then
    export CI="true"
fi

cd "${PROJECT_ROOT}"

case "${COMMAND}" in
    all)
        run_all_tests
        ;;
    unit)
        run_unit_tests
        ;;
    static)
        run_static_analysis
        ;;
    firmware)
        run_firmware_build
        ;;
    integration)
        run_integration_tests
        ;;
    coverage)
        run_coverage
        ;;
    shell)
        open_shell
        ;;
    clean)
        clean_up
        ;;
    build)
        build_images
        ;;
    *)
        log_error "Unknown command: ${COMMAND}"
        show_help
        exit 1
        ;;
esac
