file(GLOB WINDOW_SOURCES *.cpp *.h)
add_library(WindowWrapper STATIC ${WINDOW_SOURCES})
target_link_libraries(WindowWrapper PUBLIC ${GLFW_LIBRARY} VkWrapper::VkWrapper)