#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_TYPE="${2:-Release}"

case "${1}" in
    linux)
        BUILD_DIR="${PROJECT_DIR}/build-linux"
        mkdir -p "${BUILD_DIR}"
        cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
            -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
        cmake --build "${BUILD_DIR}" -j"$(nproc)"
        echo "Build complete: ${BUILD_DIR}/twilight_game_binary"
        ;;
    win64)
        BUILD_DIR="${PROJECT_DIR}/build-win64"
        mkdir -p "${BUILD_DIR}"
        cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
            -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
            -DCMAKE_TOOLCHAIN_FILE="${PROJECT_DIR}/cmake/mingw-w64-toolchain.cmake"
        cmake --build "${BUILD_DIR}" -j"$(nproc)"
        echo "Build complete: ${BUILD_DIR}/twilight_game_binary.exe"
        ;;
    android)
        export ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"
        export ANDROID_NDK_HOME="${ANDROID_HOME}/ndk/27.2.12479018"
        # Prefer JDK 17 (full JDK with jlink) for Android builds
        if [ -d "/usr/lib/jvm/java-17-openjdk-amd64" ]; then
            export JAVA_HOME="/usr/lib/jvm/java-17-openjdk-amd64"
        fi
        ANDROID_DIR="${PROJECT_DIR}/android"

        if [ ! -d "${ANDROID_NDK_HOME}" ]; then
            echo "Error: Android NDK not found at ${ANDROID_NDK_HOME}"
            echo "Install with: sdkmanager 'ndk;27.2.12479018'"
            exit 1
        fi

        # Compile shaders to SPIR-V for bundling into APK assets
        SHADER_DIR="${PROJECT_DIR}/shaders"
        GLSLANG=$(command -v glslangValidator || echo "${ANDROID_HOME}/shader-tools/glslangValidator")
        if [ -x "${GLSLANG}" ]; then
            echo "Compiling shaders..."
            "${GLSLANG}" -V "${SHADER_DIR}/sprite.vert" -o "${SHADER_DIR}/sprite.vert.spv" 2>&1
            "${GLSLANG}" -V "${SHADER_DIR}/sprite.frag" -o "${SHADER_DIR}/sprite.frag.spv" 2>&1
        else
            echo "Warning: glslangValidator not found, using pre-compiled shaders"
        fi

        # Copy compiled shaders and assets into android assets directory
        ASSETS_DIR="${ANDROID_DIR}/app/src/main/assets"
        mkdir -p "${ASSETS_DIR}/shaders"
        cp -f "${SHADER_DIR}"/*.spv "${ASSETS_DIR}/shaders/" 2>/dev/null || true

        echo "Building Android APK (${BUILD_TYPE})..."
        cd "${ANDROID_DIR}"

        if [ "${BUILD_TYPE}" = "Debug" ]; then
            ./gradlew assembleDebug -Pndkdir="${ANDROID_NDK_HOME}"
            APK="${ANDROID_DIR}/app/build/outputs/apk/debug/app-debug.apk"
        else
            ./gradlew assembleRelease -Pndkdir="${ANDROID_NDK_HOME}"
            APK="${ANDROID_DIR}/app/build/outputs/apk/release/app-release.apk"
        fi

        if [ -f "${APK}" ]; then
            echo "Build complete: ${APK}"
        else
            echo "Build complete. Check android/app/build/outputs/apk/ for APK."
        fi
        ;;
    clean)
        rm -rf "${PROJECT_DIR}"/build-*
        rm -rf "${PROJECT_DIR}/android/app/build"
        rm -rf "${PROJECT_DIR}/android/.gradle"
        rm -rf "${PROJECT_DIR}/android/build"
        rm -f "${PROJECT_DIR}"/shaders/*.spv
        echo "Build directories cleaned."
        ;;
    all)
        echo "=== Building all platforms ==="
        echo ""
        echo "── Linux ──"
        "${PROJECT_DIR}/build.sh" linux "${BUILD_TYPE}"
        echo ""
        echo "── Windows (win64) ──"
        "${PROJECT_DIR}/build.sh" win64 "${BUILD_TYPE}"
        echo ""
        echo "── Android ──"
        "${PROJECT_DIR}/build.sh" android "${BUILD_TYPE}"
        echo ""
        echo "=== All platforms built ==="
        ;;
    *)
        echo "Usage: $0 {linux|win64|android|all|clean} [Debug|Release]"
        exit 1
        ;;
esac
