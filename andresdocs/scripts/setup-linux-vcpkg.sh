#!/usr/bin/env bash
# Created by Antigravity
# SPDX-License-Identifier: GPL-2.0-or-later
# Helper script to set up vcpkg and build environment for DOSBox-Staging on Linux

set -euo pipefail

echo "=== DOSBox-Staging Linux & vcpkg Setup Helper ==="

# 1. Check for required build tools
echo "[1/4] Checking required build dependencies..."
MISSING_TOOLS=()
for cmd in git cmake pkg-config gcc g++; do
    if ! command -v "$cmd" &>/dev/null; then
        MISSING_TOOLS+=("$cmd")
    fi
done

if [ ${#MISSING_TOOLS[@]} -gt 0 ]; then
    echo "Warning: Missing tool(s): ${MISSING_TOOLS[*]}"
    echo "On Debian/Ubuntu, install with:"
    echo "  sudo apt-get install git build-essential pkg-config cmake curl ninja-build \\"
    echo "               autoconf autoconf-archive automake bison libtool libgl1-mesa-dev \\"
    echo "               libsdl3-dev python3-venv"
fi

# 2. Setup vcpkg in $HOME/vcpkg if not already present
VCPKG_DIR="${VCPKG_ROOT:-$HOME/vcpkg}"

if [ ! -d "$VCPKG_DIR" ]; then
    echo "[2/4] Cloning vcpkg to $VCPKG_DIR..."
    git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
    echo "Bootstrapping vcpkg..."
    "$VCPKG_DIR/bootstrap-vcpkg.sh" -disableMetrics
else
    echo "[2/4] vcpkg found at $VCPKG_DIR"
fi

export VCPKG_ROOT="$VCPKG_DIR"

# 3. Inform user about shell environment export
echo "[3/4] Exporting VCPKG_ROOT=$VCPKG_ROOT"
if ! grep -q "VCPKG_ROOT" ~/.bashrc 2>/dev/null; then
    echo "Tip: Consider adding 'export VCPKG_ROOT=$VCPKG_ROOT' to your ~/.bashrc"
fi

# 4. Generate CMake presets guidance
echo "[4/4] Setup complete! You can now configure and build DOSBox-Staging:"
echo ""
echo "--- Build Commands (Debug mode using vcpkg dependencies) ---"
echo "  export VCPKG_ROOT=\"$VCPKG_DIR\""
echo "  cmake --preset=debug-linux-vcpkg"
echo "  cmake --build --preset=debug-linux-vcpkg"
echo ""
echo "--- Run APPEND Unit Tests ---"
echo "  ctest -j 8 --preset debug-linux-vcpkg -R DOS_AppendTest -V"
echo "  # or run directly with colored output:"
echo "  ./build/debug-linux-vcpkg/tests/dosbox_tests --gtest_filter=\"DosAppendTest.*\""
echo ""
