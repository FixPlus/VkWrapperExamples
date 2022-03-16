file(GLOB TEST_SOURCE *.cpp *.h)
add_library(TEST_OBJECTS OBJECT ${TEST_SOURCE})
target_link_libraries(TEST_OBJECTS ${GLFW_LIBRARY} IMGUI)