cmake_minimum_required(VERSION 3.20)
project(twilight_engine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# Platform detection
option(EB_ANDROID "Build for Android" OFF)
option(EB_BUILD_TESTS "Build tests" ON)

# ── Vulkan ──
find_package(Vulkan REQUIRED)

# ── GLFW (desktop only) ──
if(NOT EB_ANDROID)
    find_package(glfw3 3.3 QUIET)
    if(NOT glfw3_FOUND)
        message(STATUS "GLFW not found, fetching via FetchContent...")
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
        )
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)
    endif()
endif()

# ── GLM ──
find_package(glm QUIET)
if(NOT glm_FOUND)
    message(STATUS "glm not found, fetching via FetchContent...")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
    )
    FetchContent_MakeAvailable(glm)
endif()

# ── Shader compilation ──
include(cmake/CompileShaders.cmake)

# ── Engine static library ──
add_library(tw_engine STATIC
    src/engine/core/engine.cpp
    src/engine/graphics/vulkan_context.cpp
    src/engine/graphics/renderer.cpp
    src/engine/graphics/sprite_batch.cpp
    src/engine/graphics/texture.cpp
    src/engine/graphics/pipeline.cpp
    src/third_party/stb_image_impl.cpp
)

if(NOT EB_ANDROID)
    target_sources(tw_engine PRIVATE
        src/engine/platform/platform_desktop.cpp
    )
endif()

target_include_directories(tw_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party
)

target_link_libraries(tw_engine PUBLIC
    Vulkan::Vulkan
    glm::glm
)

if(NOT EB_ANDROID)
    target_link_libraries(tw_engine PUBLIC glfw)
endif()

# ── Desktop executable ──
if(NOT EB_ANDROID)
    add_executable(twilight_game_binary src/main.cpp)
    target_link_libraries(twilight_game_binary PRIVATE tw_engine)

    # Compile shaders
    compile_shaders(TARGET twilight_game_binary
        SHADERS
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.vert
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.frag
    )
endif()
