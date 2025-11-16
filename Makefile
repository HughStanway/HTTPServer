BUILD_DIR := build
SRC_DIRS := src include tests
EXECUTABLE_DIR := src
EXECUTABLE := http_server
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

format:
	clang-format -i $(SOURCES)

tidy:
	clang-tidy -p build $(shell find src include -name '*.cpp')