#!/usr/bin/env bash
# =============================================================================
# build.sh — One-command build for UV-K5 V3 Messenger firmware
#
# Prerequisites (install once):
#   Ubuntu/Debian:  sudo apt install git cmake ninja-build gcc python3 docker.io
#   macOS:          brew install git cmake ninja gcc python3 && install Docker Desktop
#   Docker must be running.
#
# Usage:
#   chmod +x build.sh
#   ./build.sh              # builds Fusion-Messenger preset
#   ./build.sh scrambler    # builds Fusion-Messenger-Scrambler preset
#   ./build.sh clean        # remove build directory
#
# Output:
#   build/Fusion-Messenger/uvk5v3-messenger.bin   ← flash this file
#   build/Fusion-Messenger/uvk1-messenger.bin     ← or this for UV-K1
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT_DIR="$SCRIPT_DIR"   # this script lives in the port root

TARGET_REPO="https://github.com/armel/uv-k1-k5v3-firmware-custom.git"
TARGET_DIR="$PORT_DIR/target-firmware"
BUILD_PRESET="${1:-Fusion-Messenger}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# ---- Handle clean -----------------------------------------------------------
if [[ "${1:-}" == "clean" ]]; then
    info "Cleaning build directory..."
    rm -rf "$TARGET_DIR/build"
    info "Done."
    exit 0
fi

# ---- Check Docker -----------------------------------------------------------
if ! docker info &>/dev/null; then
    error "Docker is not running. Please start Docker and try again."
fi

# ---- Clone target firmware --------------------------------------------------
if [[ ! -d "$TARGET_DIR/.git" ]]; then
    info "Cloning target firmware (armel/uv-k1-k5v3-firmware-custom)..."
    git clone --depth=1 "$TARGET_REPO" "$TARGET_DIR"
else
    info "Target firmware already cloned. Pulling latest..."
    git -C "$TARGET_DIR" pull --ff-only || warn "Could not pull; using existing clone."
fi

# ---- Copy port files --------------------------------------------------------
info "Copying port source files into target firmware..."

# Helper/
mkdir -p "$TARGET_DIR/Helper"
cp "$PORT_DIR/Helper/fsk.h"      "$TARGET_DIR/Helper/"
cp "$PORT_DIR/Helper/fsk.c"      "$TARGET_DIR/Helper/"
cp "$PORT_DIR/Helper/crypto.h"   "$TARGET_DIR/Helper/"
cp "$PORT_DIR/Helper/crypto.c"   "$TARGET_DIR/Helper/"

# App/
cp "$PORT_DIR/App/messenger.h"   "$TARGET_DIR/App/"
cp "$PORT_DIR/App/messenger.c"   "$TARGET_DIR/App/"
cp "$PORT_DIR/App/bk4819_fsk.c"  "$TARGET_DIR/App/"
cp "$PORT_DIR/App/scrambler.h"   "$TARGET_DIR/App/"

info "Files copied."

# ---- Apply CMakeLists patch -------------------------------------------------
info "Patching CMakeLists.txt..."

CMAKE_FILE="$TARGET_DIR/CMakeLists.txt"
MARKER="# --- MESSENGER PORT PATCH ---"

if grep -q "$MARKER" "$CMAKE_FILE"; then
    warn "CMakeLists.txt already patched — skipping."
else
    # Append our options/sources block at the end of the file
    cat >> "$CMAKE_FILE" << 'CMAKEPATCH'

# --- MESSENGER PORT PATCH ---
# Added by build.sh from uvk5v3-messenger-port

option(ENABLE_MESSENGER         "FSK SMS Messenger"                     OFF)
option(ENABLE_MESSENGER_FSK_MUTE "Mute audio during FSK RX"            ON)
option(ENABLE_ENCRYPTION        "ChaCha20 encryption for Messenger"     OFF)
option(ENABLE_SCRAMBLER         "Frequency-inversion voice scrambler"   OFF)

if(ENABLE_MESSENGER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_MESSENGER=1)
    target_sources(${PROJECT_NAME} PRIVATE
        App/messenger.c
        App/bk4819_fsk.c
        Helper/fsk.c
    )
    if(ENABLE_MESSENGER_FSK_MUTE)
        target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_MESSENGER_FSK_MUTE=1)
    endif()
    if(ENABLE_ENCRYPTION)
        target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_ENCRYPTION=1)
        target_sources(${PROJECT_NAME} PRIVATE Helper/crypto.c)
        target_link_libraries(${PROJECT_NAME} m)
    endif()
endif()

if(ENABLE_SCRAMBLER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_SCRAMBLER=1 SCRAMBLER_IMPL=1)
    target_link_libraries(${PROJECT_NAME} m)
endif()
CMAKEPATCH
    info "CMakeLists.txt patched."
fi

# ---- Add CMake presets ------------------------------------------------------
PRESETS_FILE="$TARGET_DIR/CMakePresets.json"
if grep -q "Fusion-Messenger" "$PRESETS_FILE" 2>/dev/null; then
    warn "CMakePresets.json already contains Fusion-Messenger — skipping."
else
    info "Adding Fusion-Messenger presets to CMakePresets.json..."
    # Use Python to safely edit JSON
    python3 << PYEOF
import json, sys

with open("$PRESETS_FILE", "r") as f:
    data = json.load(f)

new_presets = [
    {
        "name": "Fusion-Messenger",
        "displayName": "UV-K5 V3 / K1 — Messenger + Encryption",
        "inherits": "Fusion",
        "cacheVariables": {
            "ENABLE_MESSENGER":           {"type": "BOOL", "value": "ON"},
            "ENABLE_MESSENGER_FSK_MUTE":  {"type": "BOOL", "value": "ON"},
            "ENABLE_ENCRYPTION":          {"type": "BOOL", "value": "ON"},
            "ENABLE_SCRAMBLER":           {"type": "BOOL", "value": "OFF"}
        }
    },
    {
        "name": "Fusion-Messenger-Scrambler",
        "displayName": "UV-K5 V3 / K1 — Messenger + Encryption + Scrambler",
        "inherits": "Fusion-Messenger",
        "cacheVariables": {
            "ENABLE_SCRAMBLER": {"type": "BOOL", "value": "ON"}
        }
    }
]

# Find the configurePresets array
key = "configurePresets" if "configurePresets" in data else "buildPresets"
if key not in data:
    data[key] = []
data[key].extend(new_presets)

with open("$PRESETS_FILE", "w") as f:
    json.dump(data, f, indent=2)
    f.write("\n")

print("Presets added.")
PYEOF
fi

# ---- Print manual-patch reminder -------------------------------------------
cat << 'REMINDER'

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 MANUAL PATCHES REQUIRED before the firmware will compile:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 1. App/settings.h  — add Messenger fields to t_Settings struct
    (see: patch/settings_patch.h for exact code to paste)

 2. App/app.c       — add MESSENGER_Init(), MESSENGER_TimeSlice10ms(),
    MESSENGER_HandleKey() calls
    (see: patch/app_patch.c for exact insertion points)

 3. App/menu.c / App/menu.h — add menu items
    (see: patch/menu_patch.c for exact code)

 4. App/ui/status.c — call MESSENGER_DrawStatusIcon() in status bar

 After patches, run this script again to compile.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

REMINDER

# ---- Ask user if they want to proceed to build (they may need to patch first)
read -r -p "Have you applied the manual patches above? Build now? [y/N] " answer
if [[ "${answer,,}" != "y" ]]; then
    info "Build skipped. Apply patches then re-run this script."
    exit 0
fi

# ---- Build using Docker (same method as official Fusion builds) -------------
info "Building preset: $BUILD_PRESET"

cd "$TARGET_DIR"

# Fusion's compile-with-docker.sh script handles toolchain + Docker
if [[ ! -f "compile-with-docker.sh" ]]; then
    error "compile-with-docker.sh not found in $TARGET_DIR"
fi

chmod +x compile-with-docker.sh
./compile-with-docker.sh "$BUILD_PRESET"

# ---- Locate output .bin files -----------------------------------------------
info "Build complete. Locating .bin files..."

BIN_FILES=()
while IFS= read -r -d '' f; do
    BIN_FILES+=("$f")
done < <(find "$TARGET_DIR/build" -name "*.bin" -print0 2>/dev/null)

if [[ ${#BIN_FILES[@]} -eq 0 ]]; then
    error "No .bin files found. Check build output above."
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " FLASHABLE .BIN FILES:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
for f in "${BIN_FILES[@]}"; do
    size=$(du -sh "$f" | cut -f1)
    echo "  $size  $f"
done
echo ""
echo " HOW TO FLASH:"
echo "  1. Open https://armel.github.io/uvtools2/ in Chrome/Edge"
echo "  2. Click 'Flash Firmware'"
echo "  3. Put radio in DFU mode (see Fusion wiki)"
echo "  4. Select the .bin file above and flash"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
