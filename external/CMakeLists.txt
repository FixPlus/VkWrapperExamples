file(GLOB IMGUI_SOURCES imgui/*.cpp imgui/*.h)
add_library(IMGUI STATIC ${IMGUI_SOURCES})