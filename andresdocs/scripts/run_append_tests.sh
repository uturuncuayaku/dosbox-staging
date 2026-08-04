#!/usr/bin/env bash
# Created by Antigravity

set -e

BUILD_DIR="build/debug-linux"

echo "=== Building DOSBox-Staging Test Binary ==="
ninja -C "$BUILD_DIR" dosbox_tests

echo ""
echo "=== Running APPEND GTest Suite ==="
"$BUILD_DIR/tests/dosbox_tests" --gtest_filter="DosAppendTest.*"

echo ""
echo "=========================================="
echo "✅ APPEND Tests Execution Complete!"
echo "=========================================="

# Terminal bell alert to notify user
echo -e "\a"
