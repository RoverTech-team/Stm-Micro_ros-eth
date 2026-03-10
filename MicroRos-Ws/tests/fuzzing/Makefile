CC = clang
CFLAGS = -g -O1 -fno-omit-frame-pointer
SANITIZER_FLAGS = -fsanitize=address,undefined -fno-sanitize-recover=all
FUZZER_FLAGS = -fsanitize=fuzzer
COVERAGE_FLAGS = -fprofile-instr-generate -fcoverage-mapping
STANDALONE_SAN_FLAGS = -fsanitize=address,undefined

INCLUDES = -I../host/mocks -I../host/unity

SRC_DIR = .
BUILD_DIR = build
CORPUS_DIR = corpus
CRASH_DIR = crashes

MOCK_SOURCES = ../host/mocks/mock_udp_transport.c \
               ../host/mocks/mock_microros_allocators.c

FUZZ_UDP_SRC = fuzz_udp_transport.c
FUZZ_ETHERNET_SRC = fuzz_ethernet_frame.c
FUZZ_ALLOCATORS_SRC = fuzz_microros_allocators.c

FUZZ_UDP_BIN = $(BUILD_DIR)/fuzz_udp_transport
FUZZ_ETHERNET_BIN = $(BUILD_DIR)/fuzz_ethernet_frame
FUZZ_ALLOCATORS_BIN = $(BUILD_DIR)/fuzz_microros_allocators

FUZZ_UDP_COV = $(BUILD_DIR)/fuzz_udp_transport_cov
FUZZ_ETHERNET_COV = $(BUILD_DIR)/fuzz_ethernet_frame_cov
FUZZ_ALLOCATORS_COV = $(BUILD_DIR)/fuzz_microros_allocators_cov

STANDALONE_HARNESS = fuzz_test_harness.c
STANDALONE_BIN = $(BUILD_DIR)/fuzz_test_harness

.PHONY: all clean fuzz_udp fuzz_ethernet fuzz_allocators coverage run_udp run_ethernet run_allocators run_all standalone run_standalone

all: $(BUILD_DIR) $(FUZZ_UDP_BIN) $(FUZZ_ETHERNET_BIN) $(FUZZ_ALLOCATORS_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(CRASH_DIR)

$(FUZZ_UDP_BIN): $(FUZZ_UDP_SRC) $(MOCK_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) $(FUZZER_FLAGS) $(INCLUDES) \
		$(FUZZ_UDP_SRC) $(MOCK_SOURCES) -o $@

$(FUZZ_ETHERNET_BIN): $(FUZZ_ETHERNET_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) $(FUZZER_FLAGS) $(INCLUDES) \
		$(FUZZ_ETHERNET_SRC) -o $@

$(FUZZ_ALLOCATORS_BIN): $(FUZZ_ALLOCATORS_SRC) $(MOCK_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) $(FUZZER_FLAGS) $(INCLUDES) \
		$(FUZZ_ALLOCATORS_SRC) $(MOCK_SOURCES) -o $@

$(FUZZ_UDP_COV): $(FUZZ_UDP_SRC) $(MOCK_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) $(INCLUDES) \
		$(FUZZ_UDP_SRC) $(MOCK_SOURCES) -o $@

$(FUZZ_ETHERNET_COV): $(FUZZ_ETHERNET_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) $(INCLUDES) \
		$(FUZZ_ETHERNET_SRC) -o $@

$(FUZZ_ALLOCATORS_COV): $(FUZZ_ALLOCATORS_SRC) $(MOCK_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) $(INCLUDES) \
		$(FUZZ_ALLOCATORS_SRC) $(MOCK_SOURCES) -o $@

fuzz_udp: $(FUZZ_UDP_BIN)

fuzz_ethernet: $(FUZZ_ETHERNET_BIN)

fuzz_allocators: $(FUZZ_ALLOCATORS_BIN)

coverage: $(FUZZ_UDP_COV) $(FUZZ_ETHERNET_COV) $(FUZZ_ALLOCATORS_COV)

run_udp: $(FUZZ_UDP_BIN)
	ASAN_OPTIONS=abort_on_error=1:halt_on_error=1 \
	UBSAN_OPTIONS=abort_on_error=1:halt_on_error=1 \
	$(FUZZ_UDP_BIN) \
		-max_len=2048 \
		-max_total_time=60 \
		-artifact_prefix=$(CRASH_DIR)/udp_ \
		$(CORPUS_DIR)/

run_ethernet: $(FUZZ_ETHERNET_BIN)
	ASAN_OPTIONS=abort_on_error=1:halt_on_error=1 \
	UBSAN_OPTIONS=abort_on_error=1:halt_on_error=1 \
	$(FUZZ_ETHERNET_BIN) \
		-max_len=1518 \
		-max_total_time=60 \
		-artifact_prefix=$(CRASH_DIR)/ethernet_ \
		$(CORPUS_DIR)/

run_allocators: $(FUZZ_ALLOCATORS_BIN)
	ASAN_OPTIONS=abort_on_error=1:halt_on_error=1 \
	UBSAN_OPTIONS=abort_on_error=1:halt_on_error=1 \
	$(FUZZ_ALLOCATORS_BIN) \
		-max_len=1024 \
		-max_total_time=60 \
		-artifact_prefix=$(CRASH_DIR)/allocators_ \
		$(CORPUS_DIR)/

run_all: run_udp run_ethernet run_allocators

run_udp_quick: $(FUZZ_UDP_BIN)
	ASAN_OPTIONS=abort_on_error=1:halt_on_error=1 \
	$(FUZZ_UDP_BIN) \
		-max_len=2048 \
		-runs=1000 \
		-artifact_prefix=$(CRASH_DIR)/udp_ \
		$(CORPUS_DIR)/

run_ethernet_quick: $(FUZZ_ETHERNET_BIN)
	ASAN_OPTIONS=abort_on_error=1:halt_on_error=1 \
	$(FUZZ_ETHERNET_BIN) \
		-max_len=1518 \
		-runs=1000 \
		-artifact_prefix=$(CRASH_DIR)/ethernet_ \
		$(CORPUS_DIR)/

run_allocators_quick: $(FUZZ_ALLOCATORS_BIN)
	ASAN_OPTIONS=abort_on_error=1:halt_on_error=1 \
	$(FUZZ_ALLOCATORS_BIN) \
		-max_len=1024 \
		-runs=1000 \
		-artifact_prefix=$(CRASH_DIR)/allocators_ \
		$(CORPUS_DIR)/

test_crashes: $(FUZZ_UDP_BIN) $(FUZZ_ETHERNET_BIN) $(FUZZ_ALLOCATORS_BIN)
	@if [ -d "$(CRASH_DIR)" ] && [ "$$(ls -A $(CRASH_DIR) 2>/dev/null)" ]; then \
		echo "Testing crash inputs..."; \
		for crash in $(CRASH_DIR)/*; do \
			if [ -f "$$crash" ]; then \
				echo "Testing crash: $$crash"; \
				if echo "$$crash" | grep -q "udp"; then \
					$(FUZZ_UDP_BIN) "$$crash" || echo "Reproduced crash: $$crash"; \
				elif echo "$$crash" | grep -q "ethernet"; then \
					$(FUZZ_ETHERNET_BIN) "$$crash" || echo "Reproduced crash: $$crash"; \
				elif echo "$$crash" | grep -q "allocators"; then \
					$(FUZZ_ALLOCATORS_BIN) "$$crash" || echo "Reproduced crash: $$crash"; \
				fi; \
			fi; \
		done; \
	else \
		echo "No crash inputs found."; \
	fi

merge_corpus: all
	$(FUZZ_UDP_BIN) -merge=1 $(CORPUS_DIR)/ $(BUILD_DIR)/corpus_udp/ 2>/dev/null || true
	$(FUZZ_ETHERNET_BIN) -merge=1 $(CORPUS_DIR)/ $(BUILD_DIR)/corpus_ethernet/ 2>/dev/null || true
	$(FUZZ_ALLOCATORS_BIN) -merge=1 $(CORPUS_DIR)/ $(BUILD_DIR)/corpus_allocators/ 2>/dev/null || true

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(CRASH_DIR)
	rm -f *.profraw *.profdata
	find . -name "*.gcda" -delete
	find . -name "*.gcno" -delete

distclean: clean
	rm -rf corpus_merged

standalone: $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(STANDALONE_SAN_FLAGS) $(INCLUDES) \
		-DLLVMFuzzerTestOneInput=LLVMFuzzerTestOneInput_udp \
		-c $(FUZZ_UDP_SRC) -o $(BUILD_DIR)/fuzz_udp.o
	$(CC) $(CFLAGS) $(STANDALONE_SAN_FLAGS) $(INCLUDES) \
		-DLLVMFuzzerTestOneInput=LLVMFuzzerTestOneInput_ethernet \
		-c $(FUZZ_ETHERNET_SRC) -o $(BUILD_DIR)/fuzz_ethernet.o
	$(CC) $(CFLAGS) $(STANDALONE_SAN_FLAGS) $(INCLUDES) \
		-DLLVMFuzzerTestOneInput=LLVMFuzzerTestOneInput_allocators \
		-c $(FUZZ_ALLOCATORS_SRC) -o $(BUILD_DIR)/fuzz_allocators.o
	$(CC) $(CFLAGS) $(STANDALONE_SAN_FLAGS) $(INCLUDES) \
		-c ../host/mocks/mock_udp_transport.c -o $(BUILD_DIR)/mock_udp.o
	$(CC) $(CFLAGS) $(STANDALONE_SAN_FLAGS) $(INCLUDES) \
		-c ../host/mocks/mock_microros_allocators.c -o $(BUILD_DIR)/mock_allocators.o
	$(CC) $(CFLAGS) $(STANDALONE_SAN_FLAGS) $(INCLUDES) \
		$(STANDALONE_HARNESS) \
		$(BUILD_DIR)/fuzz_udp.o \
		$(BUILD_DIR)/fuzz_ethernet.o \
		$(BUILD_DIR)/fuzz_allocators.o \
		$(BUILD_DIR)/mock_udp.o \
		$(BUILD_DIR)/mock_allocators.o \
		-o $(STANDALONE_BIN)

run_standalone: standalone
	ASAN_OPTIONS=allocator_may_return_null=1 \
	UBSAN_OPTIONS=print_stacktrace=1 \
	$(STANDALONE_BIN) --all