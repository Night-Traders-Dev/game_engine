function(compile_shaders)
    cmake_parse_arguments(SHADER "" "TARGET" "SHADERS" ${ARGN})

    find_program(GLSLANG_VALIDATOR glslangValidator HINTS
        $ENV{VULKAN_SDK}/bin
        /usr/bin
        /usr/local/bin
    )

    if(NOT GLSLANG_VALIDATOR)
        message(WARNING "glslangValidator not found, shaders will not be compiled")
        return()
    endif()

    set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

    foreach(SHADER_SRC ${SHADER_SHADERS})
        get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
        set(SHADER_SPV "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT ${SHADER_SPV}
            COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_SRC} -o ${SHADER_SPV}
            DEPENDS ${SHADER_SRC}
            COMMENT "Compiling shader ${SHADER_NAME}"
        )

        list(APPEND SHADER_OUTPUTS ${SHADER_SPV})
    endforeach()

    add_custom_target(${SHADER_TARGET}_shaders ALL DEPENDS ${SHADER_OUTPUTS})
    add_dependencies(${SHADER_TARGET} ${SHADER_TARGET}_shaders)
endfunction()
