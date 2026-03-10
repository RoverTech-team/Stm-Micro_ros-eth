CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -g
INCLUDES = -I. -Iunity -Imocks

BUILD_DIR = build
COVERAGE_DIR = $(BUILD_DIR)/coverage

MOCKS_OBJ = $(BUILD_DIR)/mock_lwip.o $(BUILD_DIR)/mock_freertos.o $(BUILD_DIR)/mock_hal.o \
            $(BUILD_DIR)/mock_memory.o $(BUILD_DIR)/mock_ip_config.o $(BUILD_DIR)/mock_transports.o \
            $(BUILD_DIR)/mock_udp_transport.o $(BUILD_DIR)/mock_microros_allocators.o

SOURCES = test_memory.c test_ip_config.c test_transports.c test_udp_transport.c test_microros_allocators.c
MOCK_SOURCES = mocks/mock_lwip.c mocks/mock_freertos.c mocks/mock_hal.c mocks/mock_memory.c \
               mocks/mock_ip_config.c mocks/mock_transports.c mocks/mock_udp_transport.c \
               mocks/mock_microros_allocators.c
ALL_SOURCES = $(SOURCES) $(MOCK_SOURCES)

.PHONY: all clean test test_memory test_ip_config test_transports test_udp_transport test_microros_allocators \
        static-analysis cppcheck gcc-warnings coverage coverage-build coverage-report valgrind

all: test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(COVERAGE_DIR):
	mkdir -p $(COVERAGE_DIR)

$(BUILD_DIR)/unity.o: unity/unity.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/mock_%.o: mocks/mock_%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/test_%.o: test_%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/test_memory_runner: $(BUILD_DIR)/unity.o $(MOCKS_OBJ) $(BUILD_DIR)/test_memory.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_ip_config_runner: $(BUILD_DIR)/unity.o $(MOCKS_OBJ) $(BUILD_DIR)/test_ip_config.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_transports_runner: $(BUILD_DIR)/unity.o $(MOCKS_OBJ) $(BUILD_DIR)/test_transports.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_udp_transport_runner: $(BUILD_DIR)/unity.o $(BUILD_DIR)/mock_udp_transport.o $(BUILD_DIR)/test_udp_transport.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_microros_allocators_runner: $(BUILD_DIR)/unity.o $(BUILD_DIR)/mock_microros_allocators.o $(BUILD_DIR)/test_microros_allocators.o
	$(CC) $(CFLAGS) $^ -o $@

test: $(BUILD_DIR)/test_memory_runner $(BUILD_DIR)/test_ip_config_runner $(BUILD_DIR)/test_transports_runner \
      $(BUILD_DIR)/test_udp_transport_runner $(BUILD_DIR)/test_microros_allocators_runner
	@echo "========================================="
	@echo "Running Memory Tests..."
	@echo "========================================="
	@./$(BUILD_DIR)/test_memory_runner
	@echo ""
	@echo "========================================="
	@echo "Running IP Config Tests..."
	@echo "========================================="
	@./$(BUILD_DIR)/test_ip_config_runner
	@echo ""
	@echo "========================================="
	@echo "Running Transport Tests..."
	@echo "========================================="
	@./$(BUILD_DIR)/test_transports_runner
	@echo ""
	@echo "========================================="
	@echo "Running UDP Transport Tests..."
	@echo "========================================="
	@./$(BUILD_DIR)/test_udp_transport_runner
	@echo ""
	@echo "========================================="
	@echo "Running Micro-ROS Allocator Tests..."
	@echo "========================================="
	@./$(BUILD_DIR)/test_microros_allocators_runner
	@echo ""
	@echo "========================================="
	@echo "All tests passed!"
	@echo "========================================="

test_memory: $(BUILD_DIR)/test_memory_runner
	./$(BUILD_DIR)/test_memory_runner

test_ip_config: $(BUILD_DIR)/test_ip_config_runner
	./$(BUILD_DIR)/test_ip_config_runner

test_transports: $(BUILD_DIR)/test_transports_runner
	./$(BUILD_DIR)/test_transports_runner

test_udp_transport: $(BUILD_DIR)/test_udp_transport_runner
	./$(BUILD_DIR)/test_udp_transport_runner

test_microros_allocators: $(BUILD_DIR)/test_microros_allocators_runner
	./$(BUILD_DIR)/test_microros_allocators_runner

static-analysis: gcc-warnings
	@echo ""
	@echo "========================================="
	@echo "Static Analysis Complete"
	@echo "========================================="

gcc-warnings:
	@echo "========================================="
	@echo "GCC Strict Warning Analysis"
	@echo "========================================="
	@$(CC) -Wall -Wextra -Wpedantic -Werror -std=c99 $(INCLUDES) -fsyntax-only $(ALL_SOURCES) 2>&1 || true
	@echo "GCC analysis complete."

cppcheck:
	@echo "========================================="
	@echo "Cppcheck Static Analysis"
	@echo "========================================="
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --std=c99 --inline-suppr \
			--suppress=missingIncludeSystem \
			--suppress=unusedFunction \
			-I. -Iunity -Imocks \
			$(ALL_SOURCES) 2>&1; \
		echo "Cppcheck analysis complete."; \
	else \
		echo "cppcheck not installed. Install with:"; \
		echo "  macOS:   brew install cppcheck"; \
		echo "  Ubuntu:  sudo apt-get install cppcheck"; \
		echo "  Windows: choco install cppcheck"; \
	fi

clang-tidy:
	@echo "========================================="
	@echo "Clang-Tidy Static Analysis"
	@echo "========================================="
	@if command -v clang-tidy >/dev/null 2>&1; then \
		clang-tidy $(ALL_SOURCES) -- $(INCLUDES) 2>&1; \
		echo "Clang-tidy analysis complete."; \
	else \
		echo "clang-tidy not installed. Install with:"; \
		echo "  macOS:   brew install llvm (then add to PATH)"; \
		echo "  Ubuntu:  sudo apt-get install clang-tidy"; \
	fi

coverage-build: CFLAGS += --coverage
coverage-build: clean | $(COVERAGE_DIR)
	@echo "========================================="
	@echo "Building with Coverage Instrumentation"
	@echo "========================================="
	$(CC) $(CFLAGS) $(INCLUDES) -c unity/unity.c -o $(BUILD_DIR)/unity.o
	$(CC) $(CFLAGS) $(INCLUDES) -c mocks/mock_udp_transport.c -o $(BUILD_DIR)/mock_udp_transport.o
	$(CC) $(CFLAGS) $(INCLUDES) -c mocks/mock_microros_allocators.c -o $(BUILD_DIR)/mock_microros_allocators.o
	$(CC) $(CFLAGS) $(INCLUDES) -c test_udp_transport.c -o $(BUILD_DIR)/test_udp_transport.o
	$(CC) $(CFLAGS) $(INCLUDES) -c test_microros_allocators.c -o $(BUILD_DIR)/test_microros_allocators.o
	$(CC) $(CFLAGS) $(BUILD_DIR)/unity.o $(BUILD_DIR)/mock_udp_transport.o $(BUILD_DIR)/test_udp_transport.o -o $(BUILD_DIR)/test_udp_transport_runner
	$(CC) $(CFLAGS) $(BUILD_DIR)/unity.o $(BUILD_DIR)/mock_microros_allocators.o $(BUILD_DIR)/test_microros_allocators.o -o $(BUILD_DIR)/test_microros_allocators_runner
	@echo "Coverage build complete."

coverage: coverage-build
	@echo "========================================="
	@echo "Running Tests for Coverage"
	@echo "========================================="
	@./$(BUILD_DIR)/test_udp_transport_runner
	@./$(BUILD_DIR)/test_microros_allocators_runner
	@echo ""
	@echo "========================================="
	@echo "Generating Coverage Report"
	@echo "========================================="
	@cd $(BUILD_DIR) && gcov -o . ../mocks/mock_udp_transport.c ../mocks/mock_microros_allocators.c 2>&1
	@cd $(BUILD_DIR) && gcov -o . ../test_udp_transport.c ../test_microros_allocators.c 2>&1
	@echo ""
	@echo "Coverage Summary:"
	@echo "-------------------------------------------"
	@if command -v lcov >/dev/null 2>&1; then \
		lcov --capture --directory $(BUILD_DIR) --output-file $(COVERAGE_DIR)/coverage.info 2>/dev/null; \
		lcov --list $(COVERAGE_DIR)/coverage.info 2>/dev/null || true; \
		genhtml $(COVERAGE_DIR)/coverage.info --output-directory $(COVERAGE_DIR)/html 2>/dev/null || true; \
		echo "HTML report generated at: $(COVERAGE_DIR)/html/index.html"; \
	else \
		echo "lcov not installed. Raw .gcov files available in $(BUILD_DIR)/"; \
		echo "Install lcov for HTML reports:"; \
		echo "  macOS:   brew install lcov"; \
		echo "  Ubuntu:  sudo apt-get install lcov"; \
	fi

coverage-report:
	@echo "========================================="
	@echo "Coverage Report Summary"
	@echo "========================================="
	@if [ -d "$(COVERAGE_DIR)" ]; then \
		if [ -f "$(COVERAGE_DIR)/coverage.info" ]; then \
			lcov --list $(COVERAGE_DIR)/coverage.info 2>/dev/null || echo "Run 'make coverage' first"; \
		else \
			echo "Run 'make coverage' first to generate coverage data"; \
		fi; \
	else \
		echo "Run 'make coverage' first to generate coverage data"; \
	fi

valgrind:
	@echo "========================================="
	@echo "Running Tests with Valgrind"
	@echo "========================================="
	@if command -v valgrind >/dev/null 2>&1; then \
		echo "Running test_udp_transport under valgrind..."; \
		valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
			--error-exitcode=1 ./$(BUILD_DIR)/test_udp_transport_runner 2>&1; \
		echo ""; \
		echo "Running test_microros_allocators under valgrind..."; \
		valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
			--error-exitcode=1 ./$(BUILD_DIR)/test_microros_allocators_runner 2>&1; \
		echo ""; \
		echo "Valgrind analysis complete."; \
	else \
		echo "valgrind not installed. Install with:"; \
		echo "  Linux:   sudo apt-get install valgrind"; \
		echo "  macOS:   valgrind is not well supported on macOS"; \
	fi

clean:
	rm -rf $(BUILD_DIR)
