file(GLOB RENDER_ENGINE_SOURCES *.cpp *.h RenderPassGraph/*.cpp RenderPassGraph/*.h Pipelines/*.cpp Pipelines/*.h Shaders/*.cpp Shaders/*.h AssetImport/*.cpp AssetImport/*.h)
add_library(RenderEngine STATIC ${RENDER_ENGINE_SOURCES})
add_subdirectory(Window)
target_link_libraries(RenderEngine WindowWrapper IMGUI -L${RENDERER_LIB_DIR} ${RENDERER_LIB})