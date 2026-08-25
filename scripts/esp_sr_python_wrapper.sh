#!/usr/bin/env bash
set -euo pipefail

REAL_PYTHON="${REAL_PYTHON:-/usr/bin/python3}"

# ESP-SR CMake invokes:
#   python .../esp-sr/model/movemodel.py -d1 <sdkconfig> -d2 <component> -d3 <build>
# Intercept only this packaging step. The exact ESP-SR component already
# resolved by this project is used, and Espressif's official pack_model.py
# performs the binary packaging. This avoids movemodel.py's sdkconfig parser.
if [[ "${1:-}" == */esp-sr/model/movemodel.py ]]; then
    ESP_SR_DIR=""
    BUILD_DIR=""
    previous=""
    for arg in "$@"; do
        if [[ "$previous" == "-d2" ]]; then ESP_SR_DIR="$arg"; fi
        if [[ "$previous" == "-d3" ]]; then BUILD_DIR="$arg"; fi
        previous="$arg"
    done

    test -d "$ESP_SR_DIR"
    test -n "$BUILD_DIR"

    MODEL_SRC="$ESP_SR_DIR/model/wakenet_model/wn9_hiesp"
    PACKER="$ESP_SR_DIR/model/pack_model.py"
    test -d "$MODEL_SRC"
    test -f "$MODEL_SRC/wn9_index"
    test -f "$MODEL_SRC/wn9_data"
    test -f "$PACKER"

    rm -rf "$BUILD_DIR/srmodels"
    mkdir -p "$BUILD_DIR/srmodels"
    cp -a "$MODEL_SRC" "$BUILD_DIR/srmodels/wn9_hiesp"

    "$REAL_PYTHON" -u "$PACKER" \
        -m "$BUILD_DIR/srmodels" \
        -o srmodels.bin

    test -s "$BUILD_DIR/srmodels/srmodels.bin"
    echo "ESP-SR CMake model target: official pack_model.py + exact managed ESP-SR component"
    stat -c '%n %s bytes' "$BUILD_DIR/srmodels/srmodels.bin"
    exit 0
fi

exec "$REAL_PYTHON" "$@"
