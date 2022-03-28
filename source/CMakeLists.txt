add_subdirectory(common)

file(GLOB EXAMPLES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/examples examples/*)

include(../cmake/compile_shaders.cmake)

add_library(SHADER_LIB INTERFACE)

compileShaders(${CMAKE_CURRENT_SOURCE_DIR}/shaders data/shaders SHADER_LIB data/shaders)

foreach(EXAMPLE IN ITEMS ${EXAMPLES})
    message(${EXAMPLE})
    file(GLOB EX_SOURCE examples/${EXAMPLE}/*)
    add_executable(${EXAMPLE} ${EX_SOURCE})
    target_link_libraries(${EXAMPLE} RenderKit SHADER_LIB)
    install(TARGETS ${EXAMPLE} )
endforeach()