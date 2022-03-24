file(GLOB COMMON_SOURCES *.cpp *.h)
add_library(RenderKit STATIC ${COMMON_SOURCES})
add_subdirectory(RenderEngine)
target_link_libraries(RenderKit RenderEngine)