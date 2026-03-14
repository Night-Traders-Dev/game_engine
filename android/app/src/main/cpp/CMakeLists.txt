cmake_minimum_required(VERSION 3.22)
project(twilight_game_binary_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Engine source root
set(ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")

# ── GLM (FetchContent) ──
include(FetchContent)
find_package(glm QUIET)
if(NOT glm_FOUND)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
    )
    FetchContent_MakeAvailable(glm)
endif()

# ── Find Vulkan from NDK ──
find_library(vulkan-lib vulkan)

# ── Native app glue ──
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# ── Engine sources (same files as desktop, minus desktop platform) ──
set(ENGINE_SOURCES
    ${ENGINE_ROOT}/src/engine/core/engine.cpp
    ${ENGINE_ROOT}/src/engine/graphics/vulkan_context.cpp
    ${ENGINE_ROOT}/src/engine/graphics/renderer.cpp
    ${ENGINE_ROOT}/src/engine/graphics/sprite_batch.cpp
    ${ENGINE_ROOT}/src/engine/graphics/texture.cpp
    ${ENGINE_ROOT}/src/engine/graphics/pipeline.cpp
    ${ENGINE_ROOT}/src/engine/graphics/texture_atlas.cpp
    ${ENGINE_ROOT}/src/engine/resource/resource_manager.cpp
    ${ENGINE_ROOT}/src/engine/resource/file_io.cpp
    ${ENGINE_ROOT}/src/engine/platform/platform_android.cpp
    ${ENGINE_ROOT}/src/engine/platform/touch_controls.cpp
    ${ENGINE_ROOT}/src/game/overworld/camera.cpp
    ${ENGINE_ROOT}/src/game/overworld/tile_map.cpp
    ${ENGINE_ROOT}/src/third_party/stb_image_impl.cpp
)

# ── Build as shared library (required for Android NativeActivity) ──
add_library(twilight_game_binary SHARED
    android_main.cpp
    ${ENGINE_SOURCES}
)

target_include_directories(twilight_game_binary PRIVATE
    ${ENGINE_ROOT}/src
    ${ENGINE_ROOT}/src/third_party
    ${ANDROID_NDK}/sources/android/native_app_glue
)

target_link_libraries(twilight_game_binary
    ${vulkan-lib}
    glm::glm
    native_app_glue
    android
    log
    m
)

target_compile_definitions(twilight_game_binary PRIVATE
    EB_ANDROID=1
    VK_USE_PLATFORM_ANDROID_KHR
)

# Ensure native_app_glue symbols aren't stripped
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")
