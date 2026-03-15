#!/bin/bash
# ═══════════════════════════════════════════════════════════
# Twilight Build Script — Build a game against Twilight Engine
# ═══════════════════════════════════════════════════════════
#
# Usage: ./twilight-build.sh <game> <platform> [build_type]
#
# Examples:
#   ./twilight-build.sh supernatural linux
#   ./twilight-build.sh supernatural win64 Release
#   ./twilight-build.sh supernatural android
#   ./twilight-build.sh supernatural all
#
# Games are stored in games/<game>/ with a game.json manifest.
# The build script:
#   1. Compiles the Twilight Engine
#   2. Symlinks/copies the game's assets into the build directory
#   3. Outputs the final executable

set -e

GAME="${1:-supernatural}"
PLATFORM="${2:-linux}"
BUILD_TYPE="${3:-Release}"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
GAME_DIR="${PROJECT_DIR}/games/${GAME}"

# Validate game exists
if [ ! -f "${GAME_DIR}/game.json" ]; then
    echo "Error: Game '${GAME}' not found (missing ${GAME_DIR}/game.json)"
    echo ""
    echo "Available games:"
    for g in "${PROJECT_DIR}/games"/*/game.json; do
        [ -f "$g" ] && basename "$(dirname "$g")"
    done
    exit 1
fi

echo "═══════════════════════════════════════════════"
echo "  Twilight Engine — Building: ${GAME}"
echo "  Platform: ${PLATFORM} (${BUILD_TYPE})"
echo "═══════════════════════════════════════════════"

# Helper: symlink game assets into build dir
link_game_assets() {
    local BUILD_DIR="$1"
    echo "Linking game assets from ${GAME_DIR}/assets/ ..."

    # Remove old assets symlink/dir
    rm -rf "${BUILD_DIR}/assets" 2>/dev/null

    # Create assets dir and symlink game content
    mkdir -p "${BUILD_DIR}/assets"

    # Engine assets (fonts, default textures)
    if [ -d "${PROJECT_DIR}/assets/engine" ]; then
        for d in "${PROJECT_DIR}/assets/engine"/*/; do
            local dirname=$(basename "$d")
            ln -sf "$d" "${BUILD_DIR}/assets/${dirname}" 2>/dev/null || cp -r "$d" "${BUILD_DIR}/assets/${dirname}"
        done
    fi

    # Game assets (override engine defaults)
    for d in "${GAME_DIR}/assets"/*/; do
        local dirname=$(basename "$d")
        # Remove engine default if game provides its own
        rm -rf "${BUILD_DIR}/assets/${dirname}" 2>/dev/null
        ln -sf "$d" "${BUILD_DIR}/assets/${dirname}" 2>/dev/null || cp -r "$d" "${BUILD_DIR}/assets/${dirname}"
    done

    # Copy game.json to build dir
    cp "${GAME_DIR}/game.json" "${BUILD_DIR}/game.json"

    # Also symlink shaders
    ln -sf "${PROJECT_DIR}/shaders" "${BUILD_DIR}/shaders" 2>/dev/null || true
}

build_linux() {
    local BUILD_DIR="${PROJECT_DIR}/build-linux"
    mkdir -p "${BUILD_DIR}"
    cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" 2>&1 | tail -5
    cmake --build "${BUILD_DIR}" -j$(nproc) 2>&1 | tail -10
    link_game_assets "${BUILD_DIR}"
    echo "Build complete: ${BUILD_DIR}/twilight_game_binary"
}

build_win64() {
    local BUILD_DIR="${PROJECT_DIR}/build-win64"
    mkdir -p "${BUILD_DIR}"
    cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_TOOLCHAIN_FILE="${PROJECT_DIR}/cmake/mingw-w64-toolchain.cmake" 2>&1 | tail -5
    cmake --build "${BUILD_DIR}" -j$(nproc) 2>&1 | tail -10
    link_game_assets "${BUILD_DIR}"
    echo "Build complete: ${BUILD_DIR}/twilight_game_binary.exe"
}

build_android() {
    local ANDROID_DIR="${PROJECT_DIR}/android"
    local ASSETS_DIR="${ANDROID_DIR}/app/src/main/assets"

    # Copy game assets to Android assets directory
    echo "Copying game assets to Android..."
    rm -rf "${ASSETS_DIR}/assets" 2>/dev/null
    mkdir -p "${ASSETS_DIR}/assets"

    # Engine assets
    if [ -d "${PROJECT_DIR}/assets/engine" ]; then
        for d in "${PROJECT_DIR}/assets/engine"/*/; do
            local dirname=$(basename "$d")
            mkdir -p "${ASSETS_DIR}/assets/${dirname}"
            cp -ru "$d"* "${ASSETS_DIR}/assets/${dirname}/" 2>/dev/null || true
        done
    fi

    # Game assets
    for d in "${GAME_DIR}/assets"/*/; do
        local dirname=$(basename "$d")
        mkdir -p "${ASSETS_DIR}/assets/${dirname}"
        cp -ru "$d"* "${ASSETS_DIR}/assets/${dirname}/" 2>/dev/null || true
    done

    # Copy game.json
    cp "${GAME_DIR}/game.json" "${ASSETS_DIR}/game.json"

    # Set up Android SDK/NDK paths
    export ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"
    export ANDROID_NDK_HOME="${ANDROID_HOME}/ndk/27.2.12479018"
    if [ -d "/usr/lib/jvm/java-17-openjdk-amd64" ]; then
        export JAVA_HOME="/usr/lib/jvm/java-17-openjdk-amd64"
    fi
    if [ ! -d "${ANDROID_NDK_HOME}" ]; then
        echo "Error: Android NDK not found at ${ANDROID_NDK_HOME}"
        exit 1
    fi

    echo "Building Android APK..."
    cd "${ANDROID_DIR}"
    ./gradlew assembleDebug -Pndkdir="${ANDROID_NDK_HOME}" 2>&1 | tail -5
    echo "Build complete: ${ANDROID_DIR}/app/build/outputs/apk/debug/app-debug.apk"
}

case "${PLATFORM}" in
    linux)   build_linux ;;
    win64)   build_win64 ;;
    android) build_android ;;
    all)
        build_linux
        build_win64
        build_android
        ;;
    clean)
        rm -rf build-linux build-win64
        echo "Cleaned build directories"
        ;;
    *)
        echo "Usage: $0 <game> {linux|win64|android|all|clean} [Debug|Release]"
        echo ""
        echo "Available games:"
        for g in "${PROJECT_DIR}/games"/*/game.json; do
            [ -f "$g" ] && echo "  $(basename "$(dirname "$g")")"
        done
        exit 1
        ;;
esac
