#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

FUZZ_TIME=${FUZZ_TIME:-60}
FUZZ_RUNS=${FUZZ_RUNS:-0}
MAX_LEN_UDP=${MAX_LEN_UDP:-2048}
MAX_LEN_ETHERNET=${MAX_LEN_ETHERNET:-1518}
MAX_LEN_ALLOCATORS=${MAX_LEN_ALLOCATORS:-1024}

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

check_dependencies() {
    log_info "Checking dependencies..."
    
    if ! command -v clang &> /dev/null; then
        log_error "clang not found. Please install clang."
        exit 1
    fi
    
    if ! clang -fsanitize=address -fsanitize=fuzzer -xc - -o /dev/null 2>/dev/null <<'EOF'
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len) { return 0; }
EOF
    then
        log_error "LibFuzzer support not available in clang."
        exit 1
    fi
    
    log_success "All dependencies satisfied."
}

build_fuzzers() {
    log_info "Building fuzzers..."
    make clean 2>/dev/null || true
    make all
    log_success "Fuzzers built successfully."
}

create_corpus_seeds() {
    log_info "Creating corpus seeds..."
    mkdir -p corpus
    
    if [ ! -f corpus/udp_packet.bin ]; then
        printf '\x01\x01\xc0\xa8\x01\x64\x00\x00\x00\x00\x00\x00\x00\x00test_payload' > corpus/udp_packet.bin
    fi
    
    if [ ! -f corpus/ethernet_frame.bin ]; then
        printf '\xff\xff\xff\xff\xff\xff\x00\x11\x22\x33\x44\x55\x08\x00\x45\x00\x00\x14\x00\x00\x00\x00\x40\x11\x00\x00\xc0\xa8\x01\x01\xc0\xa8\x01\x02' > corpus/ethernet_frame.bin
    fi
    
    if [ ! -f corpus/microros_message.bin ]; then
        printf '\x00\x01\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x03\x04\x05' > corpus/microros_message.bin
    fi
    
    for i in $(seq 0 9); do
        printf "\x01\x$(printf '%02x' $i)\xc0\xa8\x01\x64" > "corpus/udp_seed_$i.bin" 2>/dev/null || true
    done
    
    for i in $(seq 0 9); do
        printf "\x$(printf '%02x' $((i % 256)))\x00\x00\x00\x00" > "corpus/alloc_seed_$i.bin" 2>/dev/null || true
    done
    
    for i in $(seq 0 9); do
        head -c $((14 + 20 + 8 + i * 10)) /dev/urandom > "corpus/ethernet_seed_$i.bin" 2>/dev/null || true
    done
    
    log_success "Corpus seeds created."
}

run_fuzzer() {
    local name=$1
    local bin=$2
    local max_len=$3
    local corpus_dir=$4
    
    log_info "Running $name fuzzer..."
    log_info "  Max length: $max_len"
    log_info "  Fuzz time: ${FUZZ_TIME}s"
    
    mkdir -p crashes
    
    local opts="ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:detect_leaks=1:allocator_may_return_null=1"
    opts="$opts UBSAN_OPTIONS=abort_on_error=1:halt_on_error=1"
    
    local fuzzer_opts="-max_len=$max_len -artifact_prefix=crashes/${name}_"
    
    if [ "$FUZZ_RUNS" -gt 0 ]; then
        fuzzer_opts="$fuzzer_opts -runs=$FUZZ_RUNS"
        log_info "  Max runs: $FUZZ_RUNS"
    else
        fuzzer_opts="$fuzzer_opts -max_total_time=$FUZZ_TIME"
    fi
    
    if [ -d "$corpus_dir" ] && [ "$(ls -A $corpus_dir 2>/dev/null)" ]; then
        fuzzer_opts="$fuzzer_opts $corpus_dir"
    fi
    
    local output_file="build/${name}_output.txt"
    
    if env $opts $bin $fuzzer_opts 2>&1 | tee "$output_file"; then
        log_success "$name fuzzer completed without crashes."
    else
        local exit_code=$?
        if [ $exit_code -ne 0 ]; then
            log_warning "$name fuzzer exited with code $exit_code (possible crash found)."
        fi
    fi
    
    local crashes_found=$(ls crashes/${name}_* 2>/dev/null | wc -l | tr -d ' ')
    if [ "$crashes_found" -gt 0 ]; then
        log_warning "$name: $crashes_found crash(es) found in crashes/"
    else
        log_success "$name: No crashes found."
    fi
}

generate_coverage() {
    log_info "Generating coverage report..."
    
    make coverage || {
        log_warning "Coverage build failed, skipping coverage report."
        return 0
    }
    
    mkdir -p coverage_report
    
    for fuzzer in udp_transport ethernet_frame microros_allocators; do
        local bin="build/fuzz_${fuzzer}_cov"
        if [ -x "$bin" ]; then
            log_info "Running $fuzzer for coverage..."
            
            LLVM_PROFILE_FILE="coverage_report/${fuzzer}.profraw" \
                $bin -runs=100 corpus/ 2>/dev/null || true
            
            if [ -f "coverage_report/${fuzzer}.profraw" ]; then
                llvm-profdata merge -sparse \
                    "coverage_report/${fuzzer}.profraw" \
                    -o "coverage_report/${fuzzer}.profdata" 2>/dev/null || true
                
                llvm-cov show $bin \
                    -instr-profile="coverage_report/${fuzzer}.profdata" \
                    > "coverage_report/${fuzzer}_coverage.txt" 2>/dev/null || true
                
                llvm-cov report $bin \
                    -instr-profile="coverage_report/${fuzzer}.profdata" \
                    >> "coverage_report/summary.txt" 2>/dev/null || true
            fi
        fi
    done
    
    if [ -f "coverage_report/summary.txt" ]; then
        log_success "Coverage report generated in coverage_report/"
        cat coverage_report/summary.txt
    else
        log_warning "Coverage report generation incomplete."
    fi
}

print_summary() {
    log_info "=== Fuzzing Summary ==="
    
    echo ""
    echo "Crash inputs:"
    if [ -d "crashes" ] && [ "$(ls -A crashes 2>/dev/null)" ]; then
        ls -la crashes/
    else
        echo "  No crashes found."
    fi
    
    echo ""
    echo "Corpus statistics:"
    if [ -d "corpus" ]; then
        echo "  Total corpus files: $(ls corpus/ 2>/dev/null | wc -l)"
        echo "  Corpus size: $(du -sh corpus/ 2>/dev/null | cut -f1)"
    fi
    
    echo ""
    echo "To reproduce crashes:"
    echo "  ./build/fuzz_udp_transport crashes/udp_crash_file"
    echo "  ./build/fuzz_ethernet_frame crashes/ethernet_crash_file"
    echo "  ./build/fuzz_microros_allocators crashes/allocators_crash_file"
}

usage() {
    echo "Usage: $0 [OPTIONS] [COMMAND]"
    echo ""
    echo "Commands:"
    echo "  all         Run all fuzzers (default)"
    echo "  udp         Run UDP transport fuzzer only"
    echo "  ethernet    Run Ethernet frame fuzzer only"
    echo "  allocators  Run memory allocators fuzzer only"
    echo "  quick       Run quick fuzzing (1000 runs each)"
    echo "  coverage    Generate coverage report"
    echo "  build       Build fuzzers only"
    echo "  clean       Clean build artifacts"
    echo ""
    echo "Environment Variables:"
    echo "  FUZZ_TIME          Fuzzing time in seconds (default: 60)"
    echo "  FUZZ_RUNS          Number of runs (0 for time-based, default: 0)"
    echo "  MAX_LEN_UDP        Max input length for UDP fuzzer (default: 2048)"
    echo "  MAX_LEN_ETHERNET   Max input length for Ethernet fuzzer (default: 1518)"
    echo "  MAX_LEN_ALLOCATORS Max input length for allocators fuzzer (default: 1024)"
}

main() {
    local command=${1:-all}
    
    case "$command" in
        -h|--help|help)
            usage
            exit 0
            ;;
        clean)
            make clean
            rm -rf coverage_report
            log_success "Cleaned build artifacts."
            exit 0
            ;;
        build)
            check_dependencies
            build_fuzzers
            create_corpus_seeds
            exit 0
            ;;
        coverage)
            check_dependencies
            build_fuzzers
            create_corpus_seeds
            generate_coverage
            exit 0
            ;;
        quick)
            export FUZZ_RUNS=1000
            export FUZZ_TIME=0
            check_dependencies
            build_fuzzers
            create_corpus_seeds
            run_fuzzer "udp" "build/fuzz_udp_transport" "$MAX_LEN_UDP" "corpus"
            run_fuzzer "ethernet" "build/fuzz_ethernet_frame" "$MAX_LEN_ETHERNET" "corpus"
            run_fuzzer "allocators" "build/fuzz_microros_allocators" "$MAX_LEN_ALLOCATORS" "corpus"
            print_summary
            exit 0
            ;;
        udp)
            check_dependencies
            build_fuzzers
            create_corpus_seeds
            run_fuzzer "udp" "build/fuzz_udp_transport" "$MAX_LEN_UDP" "corpus"
            print_summary
            exit 0
            ;;
        ethernet)
            check_dependencies
            build_fuzzers
            create_corpus_seeds
            run_fuzzer "ethernet" "build/fuzz_ethernet_frame" "$MAX_LEN_ETHERNET" "corpus"
            print_summary
            exit 0
            ;;
        allocators)
            check_dependencies
            build_fuzzers
            create_corpus_seeds
            run_fuzzer "allocators" "build/fuzz_microros_allocators" "$MAX_LEN_ALLOCATORS" "corpus"
            print_summary
            exit 0
            ;;
        all|*)
            check_dependencies
            build_fuzzers
            create_corpus_seeds
            run_fuzzer "udp" "build/fuzz_udp_transport" "$MAX_LEN_UDP" "corpus"
            run_fuzzer "ethernet" "build/fuzz_ethernet_frame" "$MAX_LEN_ETHERNET" "corpus"
            run_fuzzer "allocators" "build/fuzz_microros_allocators" "$MAX_LEN_ALLOCATORS" "corpus"
            print_summary
            ;;
    esac
}

main "$@"