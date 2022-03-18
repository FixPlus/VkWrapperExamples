file(GLOB COMMON_SOURCES *.cpp *.h)
add_library(RenderKit STATIC ${COMMON_SOURCES})
target_link_libraries(RenderKit ${GLFW_LIBRARY} IMGUI -L${RENDERER_LIB_DIR} ${RENDERER_LIB})