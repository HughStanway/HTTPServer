BUILD_DIR := build
SRC_DIRS := src include tests
EXECUTABLE_DIR := src
TEST_DIR := tests
EXECUTABLE := http_server
TEST_EXECUTABLE := test_httpserver
SOURCES := $(shell find $(SRC_DIRS) -name '*.cpp' -o -name '*.hpp' -o -name '*.h')

.PHONY: all build run clean rebuild

all: build

build:
	@echo "==> Configuring and Building..."
	@cmake -S . -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR)

run: build
	@echo "==> Running $(EXECUTABLE)..."
	@./$(BUILD_DIR)/$(EXECUTABLE_DIR)/$(EXECUTABLE)

clean:
	@echo "==> Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

rebuild: clean build

test: build
	@echo "==> Running tests..."
	@./$(BUILD_DIR)/$(TEST_DIR)/$(TEST_EXECUTABLE)

format:
	clang-format -i $(SOURCES)

tidy:
	clang-tidy -p build $(shell find src include -name '*.cpp')