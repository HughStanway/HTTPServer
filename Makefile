BUILD_DIR := build
EXECUTABLE := http_server

.PHONY: all build run clean rebuild

all: build

build:
	@echo "==> Configuring and Building..."
	@cmake -S . -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR)

run: build
	@echo "==> Running $(EXECUTABLE)..."
	@./$(BUILD_DIR)/$(EXECUTABLE)

clean:
	@echo "==> Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

rebuild: clean build

