#!/bin/bash
set -e

# Script to build and install the HTTPServer library

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build_install"
INSTALL_PREFIX="${1:-/usr/local}"

echo "Building and installing HTTPServer library..."

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure the project, disabling tests and the main application
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
      -DBUILD_TESTING=OFF \
      -DBUILD_SERVER_APP=OFF \
      ..

# Build just the library
# We use a cross-platform way to get the number of cores for parallel build
CORES=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 1)
cmake --build . --target httpserver_lib -j "$CORES"

# Install
echo "Installing to prefix: $INSTALL_PREFIX"

# If installing to a system directory like /usr/local, we might need sudo
if [ ! -w "$INSTALL_PREFIX" ] && [ ! -w "$(dirname "$INSTALL_PREFIX")" ]; then
    echo "Requires sudo to install to $INSTALL_PREFIX."
    sudo cmake --install .
else
    cmake --install .
fi

echo "HTTPServer library installed successfully."
