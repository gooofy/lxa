#!/bin/bash
#
# build.sh - Clean build script for lxa
#
# This script performs a clean build of lxa using CMake with a custom
# installation prefix.
#
# Usage:
#   ./build.sh              # Build only
#   ./build.sh install      # Build and install
#   ./build.sh clean        # Clean build directory only
#

set -e  # Exit on error

# Configuration
INSTALL_PREFIX="/home/guenter/projects/amiga/lxa/usr"
BUILD_TYPE="Release"
BUILD_DIR="build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored messages
info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Handle clean command
if [ "$1" == "clean" ]; then
    info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    info "Build directory cleaned."
    exit 0
fi

# Clean and create build directory
info "Cleaning previous build..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Create install prefix directory if it doesn't exist
if [ ! -d "$INSTALL_PREFIX" ]; then
    info "Creating install prefix directory: $INSTALL_PREFIX"
    mkdir -p "$INSTALL_PREFIX"
fi

# Configure with CMake
info "Configuring with CMake..."
cd "$BUILD_DIR"
cmake \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    ..

# Build
info "Building lxa (this may take a while)..."
make -j$(nproc)

info "Build complete!"

# Show what was built
echo ""
echo "Built artifacts:"
echo "  - Host binary:    $SCRIPT_DIR/$BUILD_DIR/host/bin/lxa"
echo "  - ROM image:      $SCRIPT_DIR/$BUILD_DIR/target/rom/lxa.rom"
echo "  - C commands:     $SCRIPT_DIR/$BUILD_DIR/target/sys/C/"
echo "  - System tools:   $SCRIPT_DIR/$BUILD_DIR/target/sys/System/"

# Handle install command
if [ "$1" == "install" ]; then
    echo ""
    info "Installing to $INSTALL_PREFIX..."
    make install
    info "Installation complete!"
    echo ""
    echo "Installed to:"
    echo "  - Binary:         $INSTALL_PREFIX/bin/lxa"
    echo "  - Data files:     $INSTALL_PREFIX/share/lxa/"
fi

echo ""
echo "To run lxa:"
echo "  $SCRIPT_DIR/$BUILD_DIR/host/bin/lxa -r $SCRIPT_DIR/$BUILD_DIR/target/rom/lxa.rom <program>"
echo ""
echo "Or after install:"
echo "  $INSTALL_PREFIX/bin/lxa <program>"
