function(compileShader SHADER_NAME SHADER_DIR OUTPUT_DIR)

    add_custom_target(${SHADER_NAME} ALL COMMAND ${GLSL} -V --allow-partial-linkage -I${SHADER_DIR} -o ${OUTPUT_DIR}/${SHADER_NAME}.spv ${SHADER_DIR}/${SHADER_NAME})

endfunction(compileShader)

function(compileShaders SHADER_DIR OUTPUT_DIR DEPENDENT_TARGET INSTALL_DIR)
	find_program(GLSL NAMES glslangValidator glslangValidator.exe PATHS $ENV{VK_SDK_PATH}/Bin $ENV{VULKAN_SDK}/bin)

    if(NOT GLSL)
        message("GLSL compiler not found - shader compilation skipped")
    ELSE()
        message("Found following GLSL compiler:")
        message(STATUS ${GLSL})

        file(GLOB SHADERS RELATIVE ${SHADER_DIR}  ${SHADER_DIR}/*.vert ${SHADER_DIR}/*.frag)

        foreach(SHADER ${SHADERS})
            compileShader(${SHADER} ${SHADER_DIR} ${OUTPUT_DIR})
        endforeach(SHADER)

        list(TRANSFORM SHADERS APPEND ".spv" OUTPUT_VARIABLE SHADERS_BINS)
        list(TRANSFORM SHADERS_BINS PREPEND ${OUTPUT_DIR}/)
        install(FILES ${SHADERS_BINS} DESTINATION ${INSTALL_DIR})
        add_dependencies(${DEPENDENT_TARGET} ${SHADERS})
    ENDIF()
endfunction(compileShaders)
