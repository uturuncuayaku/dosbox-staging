#!/usr/bin/env bash
# Created by Antigravity
# SPDX-License-Identifier: GPL-2.0-or-later
# Comprehensive Linux setup script for DOSBox-Staging supporting GCC/system packages & vcpkg

set -euo pipefail

echo "======================================================="
echo "  DOSBox-Staging Linux Development Setup Helper"
echo "======================================================="

# 1. Check base build tools
echo ""
echo "[1/3] Checking base compiler & build tools..."
MISSING_TOOLS=()
for cmd in git cmake pkg-config gcc g++; do
    if ! command -v "$cmd" &>/dev/null; then
        MISSING_TOOLS+=("$cmd")
    fi
done

if [ ${#MISSING_TOOLS[@]} -gt 0 ]; then
    echo "Error: Missing core build tool(s): ${MISSING_TOOLS[*]}"
    echo "Please install base development tools (build-essential, cmake, git, pkg-config)."
    exit 1
fi
echo "  -> Base build tools (GCC, CMake, Git, pkg-config) are installed!"

# 2. Setup vcpkg by default for missing system libraries
VCPKG_MODE="${1:---vcpkg}"
VCPKG_DIR="${VCPKG_ROOT:-$HOME/vcpkg}"

if [ "$VCPKG_MODE" = "--vcpkg" ] || [ "$VCPKG_MODE" = "vcpkg" ]; then
    echo ""
    echo "[2/3] Setting up Microsoft vcpkg environment in $VCPKG_DIR..."
    echo "      (vcpkg automatically builds MT32Emu, FluidSynth, SDL3, GTest, etc.)"
    if [ ! -d "$VCPKG_DIR" ]; then
        git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
        "$VCPKG_DIR/bootstrap-vcpkg.sh" -disableMetrics
    else
        echo "  -> vcpkg repository already present at $VCPKG_DIR"
    fi
    export VCPKG_ROOT="$VCPKG_DIR"
    PRESET_DEBUG="debug-linux-vcpkg"
    PRESET_RELEASE="release-linux-vcpkg"
else
    echo ""
    echo "[2/3] System Libraries Mode selected."
    echo "      Note: Requires system packages for SDL3, MT32Emu, FluidSynth, Opus, GTest, etc."
    PRESET_DEBUG="debug-linux"
    PRESET_RELEASE="release-linux"
fi

# 3. Usage Guidance
echo ""
echo "[3/3] Ready! Run the following commands to build & test:"
echo "-------------------------------------------------------"
if [ "$VCPKG_MODE" = "--vcpkg" ] || [ "$VCPKG_MODE" = "vcpkg" ]; then
    echo "Export environment variable:"
    echo "  export VCPKG_ROOT=\"$VCPKG_DIR\""
    echo ""
fi
echo "Step 1: Configure build:"
echo "  cmake --preset=$PRESET_DEBUG"
echo ""
echo "Step 2: Compile binary:"
echo "  cmake --build --preset=$PRESET_DEBUG"
echo ""
echo "Step 3: Run APPEND Unit Tests:"
echo "  ctest -j 8 --preset $PRESET_DEBUG -R DOS_AppendTest -V"
echo ""
echo "Or run unit tests directly:"
echo "  ./build/$PRESET_DEBUG/tests/dosbox_tests --gtest_filter=\"DosAppendTest.*\""
echo "======================================================="
