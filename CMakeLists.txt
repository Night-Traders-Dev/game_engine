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
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Cross-compiling for Windows: fetch Vulkan headers and link vulkan-1 at runtime
    message(STATUS "Cross-compiling for Windows: fetching Vulkan-Headers")
    FetchContent_Declare(
        vulkan_headers
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG v1.3.290
    )
    FetchContent_MakeAvailable(vulkan_headers)

    # Generate import library from .def file
    set(VULKAN_DEF "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vulkan-1.def")
    set(VULKAN_IMPLIB "${CMAKE_CURRENT_BINARY_DIR}/libvulkan-1.a")
    add_custom_command(
        OUTPUT ${VULKAN_IMPLIB}
        COMMAND x86_64-w64-mingw32-dlltool -d ${VULKAN_DEF} -l ${VULKAN_IMPLIB}
        DEPENDS ${VULKAN_DEF}
        COMMENT "Generating Vulkan import library for Windows cross-compile"
    )
    add_custom_target(vulkan_implib DEPENDS ${VULKAN_IMPLIB})

    add_library(vulkan_crosscompile INTERFACE)
    add_dependencies(vulkan_crosscompile vulkan_implib)
    target_include_directories(vulkan_crosscompile INTERFACE
        ${vulkan_headers_SOURCE_DIR}/include
    )
    target_link_libraries(vulkan_crosscompile INTERFACE ${VULKAN_IMPLIB})
    add_library(Vulkan::Vulkan ALIAS vulkan_crosscompile)
else()
    find_package(Vulkan REQUIRED)
endif()

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
    src/engine/graphics/texture_atlas.cpp
    src/engine/graphics/pipeline.cpp
    src/engine/resource/resource_manager.cpp
    src/engine/resource/file_io.cpp
    src/engine/resource/game_manifest.cpp
    src/game/overworld/camera.cpp
    src/game/overworld/tile_map.cpp
    src/game/game.cpp
    src/game/game_io.cpp
    src/game/game_init.cpp
    src/game/game_battle.cpp
    src/game/game_render.cpp
    src/engine/scripting/script_engine.cpp
    src/engine/scripting/script_api_new.cpp
    src/engine/scripting/script_api_player.cpp
    src/engine/scripting/script_api_npc.cpp
    src/engine/scripting/script_api_map.cpp
    src/engine/scripting/script_api_ui.cpp
    src/engine/scripting/script_api_platformer.cpp
    src/engine/scripting/script_api_physics.cpp
    src/engine/scripting/script_api_systems.cpp
    src/engine/graphics/text_renderer.cpp
    src/engine/audio/audio_engine.cpp
    src/third_party/miniaudio_impl.cpp
    src/game/dialogue/dialogue_box.cpp
    src/game/ui/merchant_ui.cpp
    src/game/ai/pathfinding.cpp
    src/game/systems/day_night.cpp
    src/game/systems/survival.cpp
    src/game/systems/spawn_system.cpp
    src/game/systems/level_manager.cpp
    src/game/game_platformer.cpp
    src/game/systems/moving_platform.cpp
    src/third_party/stb_image_impl.cpp
    src/third_party/stb_truetype_impl.cpp
)


# ── SageLang scripting engine (embedded interpreter) ──
set(SAGELANG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/sagelang)
set(SAGELANG_SOURCES
    ${SAGELANG_DIR}/src/c/ast.c
    ${SAGELANG_DIR}/src/c/lexer.c
    ${SAGELANG_DIR}/src/c/parser.c
    ${SAGELANG_DIR}/src/c/interpreter.c
    ${SAGELANG_DIR}/src/c/env.c
    ${SAGELANG_DIR}/src/c/value.c
    ${SAGELANG_DIR}/src/c/gc.c
    ${SAGELANG_DIR}/src/c/stdlib.c
    ${SAGELANG_DIR}/src/c/module.c
    ${SAGELANG_DIR}/src/c/diagnostic.c
    ${SAGELANG_DIR}/src/c/typecheck.c
    ${SAGELANG_DIR}/src/c/pass.c
    ${SAGELANG_DIR}/src/c/constfold.c
    ${SAGELANG_DIR}/src/c/dce.c
    ${SAGELANG_DIR}/src/c/inline.c
    ${SAGELANG_DIR}/src/c/formatter.c
    ${SAGELANG_DIR}/src/c/linter.c
    ${SAGELANG_DIR}/src/c/sage_thread.c
    ${SAGELANG_DIR}/src/vm/vm.c
    ${SAGELANG_DIR}/src/vm/bytecode.c
    ${SAGELANG_DIR}/src/vm/program.c
    ${SAGELANG_DIR}/src/vm/runtime.c
)
add_library(sagelang STATIC ${SAGELANG_SOURCES})
target_include_directories(sagelang PUBLIC ${SAGELANG_DIR}/include ${SAGELANG_DIR}/src/vm)
target_compile_options(sagelang PRIVATE -w) # Suppress warnings from third-party code
if(UNIX AND NOT APPLE AND NOT CMAKE_CROSSCOMPILING)
    target_link_libraries(sagelang PUBLIC dl pthread m)
elseif(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(sagelang PRIVATE SAGE_NO_FFI=1 SAGE_NO_THREADS=1 SAGE_NO_NET=1)
    # Provide mkstemps implementation for mingw (SageLang asm_compile uses it outside FFI guard)
    target_sources(sagelang PRIVATE src/third_party/mingw_compat.c)
endif()

if(NOT EB_ANDROID)
    target_sources(tw_engine PRIVATE
        src/engine/platform/platform_desktop.cpp
        src/editor/tile_editor.cpp
        src/editor/tile_editor_npc_spawner.cpp
        src/editor/tile_editor_script_ide.cpp
        src/editor/tile_editor_debug.cpp
        src/editor/tile_editor_systems.cpp
        src/editor/tile_editor_ui.cpp
        src/editor/imgui_integration.cpp
        src/third_party/tinyfiledialogs.c
        # Dear ImGui (editor UI)
        src/third_party/imgui/imgui.cpp
        src/third_party/imgui/imgui_draw.cpp
        src/third_party/imgui/imgui_tables.cpp
        src/third_party/imgui/imgui_widgets.cpp
        src/third_party/imgui/imgui_demo.cpp
        src/third_party/imgui/backends/imgui_impl_glfw.cpp
        src/third_party/imgui/backends/imgui_impl_vulkan.cpp
    )
    target_include_directories(tw_engine PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/imgui/backends
    )
endif()

target_include_directories(tw_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party
)

target_link_libraries(tw_engine PUBLIC
    Vulkan::Vulkan
    glm::glm
    sagelang
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

    # Symlink assets directory into build dir so the binary can find them
    add_custom_command(TARGET twilight_game_binary POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_CURRENT_SOURCE_DIR}/games/demo/assets
            ${CMAKE_CURRENT_BINARY_DIR}/assets
        COMMENT "Symlinking assets into build directory"
    )
    # Also symlink game.json
    add_custom_command(TARGET twilight_game_binary POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_CURRENT_SOURCE_DIR}/games/demo/game.json
            ${CMAKE_CURRENT_BINARY_DIR}/game.json
        COMMENT "Symlinking game.json into build directory"
    )
endif()
