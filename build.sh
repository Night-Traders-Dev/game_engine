#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_TYPE="${2:-Release}"

# ── Update SageLang submodule to latest main ──
update_sagelang() {
    local SAGE_DIR="${PROJECT_DIR}/src/third_party/sagelang"
    if [ -d "${SAGE_DIR}/.git" ]; then
        echo "Updating SageLang to latest..."
        cd "${SAGE_DIR}"
        git fetch origin main --quiet 2>/dev/null || true
        local LOCAL=$(git rev-parse HEAD 2>/dev/null)
        local REMOTE=$(git rev-parse origin/main 2>/dev/null)
        if [ "${LOCAL}" != "${REMOTE}" ]; then
            git merge origin/main --no-edit --quiet 2>/dev/null || true
            echo "SageLang updated: $(git log --oneline -1)"
        else
            echo "SageLang up-to-date: $(git log --oneline -1)"
        fi
        cd "${PROJECT_DIR}"
    fi
}

case "${1}" in
    linux)
        update_sagelang
        BUILD_DIR="${PROJECT_DIR}/build-linux"
        mkdir -p "${BUILD_DIR}"
        cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
            -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
        cmake --build "${BUILD_DIR}" -j"$(nproc)"
        echo "Build complete: ${BUILD_DIR}/twilight_game_binary"
        ;;
    win64)
        update_sagelang
        BUILD_DIR="${PROJECT_DIR}/build-win64"
        mkdir -p "${BUILD_DIR}"
        cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
            -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
            -DCMAKE_TOOLCHAIN_FILE="${PROJECT_DIR}/cmake/mingw-w64-toolchain.cmake"
        cmake --build "${BUILD_DIR}" -j"$(nproc)"
        echo "Build complete: ${BUILD_DIR}/twilight_game_binary.exe"
        ;;
    android)
        update_sagelang
        export ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"
        # Find the latest installed NDK
        if [ -z "${ANDROID_NDK_HOME}" ]; then
            ANDROID_NDK_HOME=$(ls -d "${ANDROID_HOME}/ndk/"* 2>/dev/null | sort -V | tail -1)
        fi
        # Find the best available JDK (prefer 21, fallback to 17)
        if [ -d "/usr/lib/jvm/java-21-openjdk-amd64" ]; then
            export JAVA_HOME="/usr/lib/jvm/java-21-openjdk-amd64"
        elif [ -d "/usr/lib/jvm/java-17-openjdk-amd64" ]; then
            export JAVA_HOME="/usr/lib/jvm/java-17-openjdk-amd64"
        fi
        ANDROID_DIR="${PROJECT_DIR}/android"

        if [ ! -d "${ANDROID_NDK_HOME}" ]; then
            echo "Error: Android NDK not found."
            echo "Install with: sdkmanager 'ndk;27.2.12479018'"
            exit 1
        fi
        echo "Using NDK: ${ANDROID_NDK_HOME}"
        echo "Using JDK: ${JAVA_HOME}"

        # Compile shaders to SPIR-V for bundling into APK assets
        SHADER_DIR="${PROJECT_DIR}/shaders"
        GLSLANG=$(command -v glslangValidator 2>/dev/null || echo "${ANDROID_HOME}/shader-tools/glslangValidator")
        if [ -x "${GLSLANG}" ]; then
            echo "Compiling shaders..."
            "${GLSLANG}" -V "${SHADER_DIR}/sprite.vert" -o "${SHADER_DIR}/sprite.vert.spv" 2>&1
            "${GLSLANG}" -V "${SHADER_DIR}/sprite.frag" -o "${SHADER_DIR}/sprite.frag.spv" 2>&1
        else
            echo "Warning: glslangValidator not found, using pre-compiled shaders"
        fi

        # Copy compiled shaders and game assets into android assets directory
        ASSETS_DIR="${ANDROID_DIR}/app/src/main/assets"
        mkdir -p "${ASSETS_DIR}/shaders"
        cp -f "${SHADER_DIR}"/*.spv "${ASSETS_DIR}/shaders/" 2>/dev/null || true

        # Copy demo game assets (preserving directory structure)
        GAME_DIR="${PROJECT_DIR}/games/demo"
        if [ -d "${GAME_DIR}/assets" ]; then
            echo "Copying demo game assets..."
            cp -f "${GAME_DIR}/game.json" "${ASSETS_DIR}/" 2>/dev/null || true
            # Use rsync if available for proper recursive copy, otherwise cp -r
            if command -v rsync &>/dev/null; then
                rsync -a --delete "${GAME_DIR}/assets/" "${ASSETS_DIR}/assets/"
            else
                for subdir in textures maps fonts dialogue audio; do
                    if [ -d "${GAME_DIR}/assets/${subdir}" ]; then
                        mkdir -p "${ASSETS_DIR}/assets/${subdir}"
                        cp -ru "${GAME_DIR}/assets/${subdir}/." "${ASSETS_DIR}/assets/${subdir}/" 2>/dev/null || true
                    fi
                done
                # Scripts with subdirectories
                if [ -d "${GAME_DIR}/assets/scripts" ]; then
                    find "${GAME_DIR}/assets/scripts" -type d | while read dir; do
                        rel="${dir#${GAME_DIR}/assets/scripts}"
                        mkdir -p "${ASSETS_DIR}/assets/scripts${rel}"
                    done
                    find "${GAME_DIR}/assets/scripts" -type f -name "*.sage" | while read f; do
                        rel="${f#${GAME_DIR}/assets/}"
                        mkdir -p "$(dirname "${ASSETS_DIR}/assets/${rel}")"
                        cp -u "$f" "${ASSETS_DIR}/assets/${rel}"
                    done
                fi
                # Parallax subdirectories
                if [ -d "${GAME_DIR}/assets/textures/parallax" ]; then
                    cp -ru "${GAME_DIR}/assets/textures/parallax/." "${ASSETS_DIR}/assets/textures/parallax/" 2>/dev/null || true
                fi
                # Blender assets
                if [ -d "${GAME_DIR}/assets/blender" ]; then
                    mkdir -p "${ASSETS_DIR}/assets/blender"
                    cp -ru "${GAME_DIR}/assets/blender/." "${ASSETS_DIR}/assets/blender/" 2>/dev/null || true
                fi
            fi
            echo "Demo game assets copied"
        fi

        echo "Building Android APK (Debug)..."
        cd "${ANDROID_DIR}"

        ./gradlew assembleDebug -Pndkdir="${ANDROID_NDK_HOME}"
        APK="${ANDROID_DIR}/app/build/outputs/apk/debug/app-debug.apk"

        if [ -f "${APK}" ]; then
            echo "Build complete: ${APK}"
        else
            echo "Build complete. Check android/app/build/outputs/apk/ for APK."
        fi
        ;;
    quest)
        # Meta Quest 2/3/Pro — same as Android build, just sideload the APK
        echo "Building for Meta Quest (flat mode + controller mapping)..."
        "${PROJECT_DIR}/build.sh" android "${BUILD_TYPE}"
        APK="${PROJECT_DIR}/android/app/build/outputs/apk/debug/app-debug.apk"
        echo ""
        echo "=== Quest Build Complete ==="
        echo "APK: ${APK}"
        echo ""
        echo "To install on Quest via ADB:"
        echo "  adb install ${APK}"
        echo ""
        echo "Controller mapping:"
        echo "  Left Stick    → Move"
        echo "  A / X Button  → Confirm / Talk / Buy"
        echo "  B / Y Button  → Cancel / Close"
        echo "  Right Trigger → Run"
        echo "  Start / Menu  → Pause Menu"
        echo "  D-Pad         → Navigate menus"
        ;;
    test)
        BUILD_DIR="${PROJECT_DIR}/build-linux"
        if [ ! -f "${BUILD_DIR}/twilight_game_binary" ]; then
            echo "No build found. Building first..."
            "${PROJECT_DIR}/build.sh" linux Debug
        fi
        echo "Running test suite..."
        cd "${BUILD_DIR}"
        ./twilight_game_binary --test
        echo "Tests complete."
        ;;
    clean)
        rm -rf "${PROJECT_DIR}"/build-*
        rm -rf "${PROJECT_DIR}/android/app/build"
        rm -rf "${PROJECT_DIR}/android/app/.cxx"
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
        echo "── Android / Quest ──"
        "${PROJECT_DIR}/build.sh" android "${BUILD_TYPE}"
        echo ""
        echo "=== All platforms built (Linux, Windows, Android/Quest) ==="
        ;;
    *)
        echo "Twilight Engine Build System"
        echo ""
        echo "Usage: $0 {linux|win64|android|quest|test|all|clean} [Debug|Release]"
        echo ""
        echo "Targets:"
        echo "  linux    Build for Linux (default Release)"
        echo "  win64    Cross-compile for Windows (requires mingw-w64)"
        echo "  android  Build Android APK (requires Android SDK/NDK)"
        echo "  quest    Build for Meta Quest (same as android + sideload instructions)"
        echo "  test     Run the test suite (141 assertions)"
        echo "  all      Build linux + win64 + android"
        echo "  clean    Remove all build directories"
        echo ""
        echo "Each build auto-updates SageLang to latest main branch."
        exit 1
        ;;
esac
