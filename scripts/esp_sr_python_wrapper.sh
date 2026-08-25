#!/usr/bin/env bash
set -euo pipefail

REAL_PYTHON="${REAL_PYTHON:-/usr/bin/python3}"
MODEL_SOURCE="${GITHUB_WORKSPACE}/model-artifact/srmodels.bin"

# ESP-SR CMake invokes:
#   python .../esp-sr/model/movemodel.py -d1 <sdkconfig> -d2 <component> -d3 <build>
# The model is built separately with Espressif's official pack_model.py.
# Intercept only movemodel.py; delegate every other Python invocation unchanged.
if [[ "${1:-}" == */esp-sr/model/movemodel.py ]]; then
    BUILD_DIR=""
    previous=""
    for arg in "$@"; do
        if [[ "$previous" == "-d3" ]]; then
            BUILD_DIR="$arg"
        fi
        previous="$arg"
    done

    test -n "$BUILD_DIR"
    test -s "$MODEL_SOURCE"

    mkdir -p "$BUILD_DIR/srmodels"
    cp "$MODEL_SOURCE" "$BUILD_DIR/srmodels/srmodels.bin"
    test -s "$BUILD_DIR/srmodels/srmodels.bin"

    echo "ESP-SR CMake model target: using prebuilt official srmodels.bin"
    sha256sum "$BUILD_DIR/srmodels/srmodels.bin"
    exit 0
fi

exec "$REAL_PYTHON" "$@"
