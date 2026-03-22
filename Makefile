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
CLANG_FORMAT ?= clang-format
DOCS_DIR := docs
DOCS_BUILD_DIR := $(DOCS_DIR)/_build
DOCS_HTML_DIR := $(DOCS_BUILD_DIR)/html
DOCS_API_DIR := $(DOCS_DIR)/api
DOCS_DOXYFILE_TMP := $(DOCS_BUILD_DIR)/Doxyfile.sphinx

.PHONY: build run clean unit_test venv integration_test test format format-check format-python docs-deps docs docs-clean help

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
	@echo "==> Formatting source files in lib/..."
	@find lib -type f \( -name "*.cpp" -o -name "*.h" \) \
	-exec $(CLANG_FORMAT) -i {} +

format-check:
	@echo "==> Checking formatting..."
	@find lib -type f \( -name "*.cpp" -o -name "*.h" \) \
	-exec $(CLANG_FORMAT) --dry-run --Werror {} +

format-python:
	@echo "==> Formatting Python files..."
	@$(VENV_PYTHON) -m ruff format ${TEST_DIR}

docs-deps: venv
	@echo "==> Installing docs dependencies..."
	@$(VENV_PIP) install -r requirements-docs.txt

docs: docs-deps
	@echo "==> Cleaning docs directory..."
	@rm -rf $(DOCS_DIR)
	@mkdir -p $(DOCS_BUILD_DIR)/doxygen
	@echo "==> Generating Doxygen XML..."
	@sed \
		-e 's|^OUTPUT_DIRECTORY.*|OUTPUT_DIRECTORY       = "$(DOCS_BUILD_DIR)/doxygen"|' \
		-e 's|^GENERATE_HTML.*|GENERATE_HTML          = NO|' \
		-e 's|^GENERATE_XML.*|GENERATE_XML           = YES|' \
		-e 's|^XML_OUTPUT.*|XML_OUTPUT             = xml|' \
		-e 's|^INPUT.*|INPUT                  = lib/include lib/src|' \
		-e 's|^RECURSIVE.*|RECURSIVE              = YES|' \
		Doxyfile > $(DOCS_DOXYFILE_TMP)
	@echo "FILE_PATTERNS += httpserver" >> $(DOCS_DOXYFILE_TMP)
	@doxygen $(DOCS_DOXYFILE_TMP)
	@echo "==> Generating Sphinx site structure..."
	@$(VENV_PYTHON) scripts/generate_api_rst.py \
		--index-xml $(DOCS_BUILD_DIR)/doxygen/xml/index.xml \
		--output $(DOCS_DIR)/api.rst \
		--project HTTPServer
	@echo "==> Building Sphinx site..."
	@$(VENV_PYTHON) -m sphinx -b html $(DOCS_DIR) $(DOCS_HTML_DIR)
	@echo "==> Docs site built at $(DOCS_HTML_DIR)/index.html"

docs-clean:
	@echo "==> Cleaning docs build output..."
	@rm -rf $(DOCS_DIR)

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
	@printf "  docs              Build Doxygen + Sphinx docs site\n"
	@printf "  docs-clean        Remove docs build output\n"
	@printf "\n"
