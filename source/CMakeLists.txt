add_subdirectory(common)

file(GLOB EXAMPLES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/examples examples/*)


foreach(EXAMPLE IN ITEMS ${EXAMPLES})
    message(${EXAMPLE})
    file(GLOB EX_SOURCE examples/${EXAMPLE}/*)
    add_executable(${EXAMPLE} ${EX_SOURCE})
    target_link_libraries(${EXAMPLE} RenderKit)
    install(TARGETS ${EXAMPLE} )
endforeach()