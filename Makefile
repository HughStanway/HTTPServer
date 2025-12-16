.DEFAULT_GOAL := help

BUILD_DIR := build
SRC_DIRS := src include tests
EXECUTABLE_DIR := src
TEST_DIR := tests
UNIT_TEST_DIR := unit_tests
EXECUTABLE := http_server
UNIT_TEST_EXECUTABLE := unit_tests
VENV_DIR := .venv
PYTHON := python3.13
VENV_PYTHON := $(VENV_DIR)/bin/python
VENV_PIP := $(VENV_DIR)/bin/pip

.PHONY: build run clean unit_test venv integration_test test format tidy help

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

unit_test: build
	@echo "==> Running unit tests..."
	@./$(BUILD_DIR)/$(TEST_DIR)/$(UNIT_TEST_DIR)/$(UNIT_TEST_EXECUTABLE)

venv:
	@if [ ! -d "$(VENV_DIR)" ]; then \
		echo "==> Creating Python venv..."; \
		$(PYTHON) -m venv $(VENV_DIR); \
	fi
	@echo "==> Installing Python test dependencies..."
	@$(VENV_PIP) install --upgrade pip
	@$(VENV_PIP) install -r requirements-dev.txt

integration_test: build venv
	@echo "==> Running integration tests..."
	@$(VENV_PYTHON) -m pytest -s $(TEST_DIR)

test: unit_test integration_test

format:
	clang-format -i $(shell find $(SRC_DIRS) -name '*.cpp' -o -name '*.hpp' -o -name '*.h')

tidy:
	clang-tidy -p build $(shell find src include -name '*.cpp')

help:
	@printf "Usage: make [target]\n\n"
	@printf "Common targets:\n"
	@printf "  build             Configure and build the project\n"
	@printf "  run               Build and run the server\n"
	@printf "  clean             Remove build directory\n"
	@printf "  unit_test         Run unit tests\n"
	@printf "  integration_test  Run integration tests\n"
	@printf "  test              Run unit and integration tests\n"
	@printf "  format            Run clang-format over sources\n"
	@printf "  tidy              Run clang-tidy over sources\n"
	@printf "\n"
