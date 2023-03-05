cmake_minimum_required(VERSION 3.20)
project(TestApp)

set(CMAKE_CXX_STANDARD 20)

include_directories(external)
include_directories(external/glm)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(glfw3 3.3 REQUIRED)
find_package(VkWrapper 3.0 REQUIRED)
set(GLFW_LIBRARY glfw)

include_directories(source)
include_directories(source/common)
add_subdirectory(external)
add_subdirectory(source)

set(EXAMPLE_ASSET_PATH "./data")
set(EXAMPLE_GLTF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/gltf-samples/2.0/")

configure_file(source/common/AssetPath.inc.in AssetPath.inc)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/source/data/shaders)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/source/data/textures)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/source/data DESTINATION .)