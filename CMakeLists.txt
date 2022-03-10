cmake_minimum_required(VERSION 3.20)
project(TestApp)

set(CMAKE_CXX_STANDARD 20)

IF(WIN32)
    IF (NOT Vulkan_FOUND)
        if(DEFINED ENV{VK_SDK_PATH})
            find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS $ENV{VK_SDK_PATH}/Lib)
            IF (Vulkan_LIBRARY)
                set(Vulkan_FOUND ON)
                include_directories($ENV{VK_SDK_PATH}/Include)
                MESSAGE("Using Vulkan SDK installed here:" $ENV{VK_SDK_PATH})
            ENDIF()

            set(VULKAN_SDK_PATH $ENV{VK_SDK_PATH})
        ELSE()
            MESSAGE(FATAL_ERROR "Vulkan not found. Make sure you have Vulkan SDK installed in your system")
        ENDIF()
    ELSE()
        include_directories(${Vulkan_INCLUDE_DIR})
    ENDIF()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
ELSEIF(UNIX)
    IF (DEFINED ENV{VULKAN_SDK})
        find_library(Vulkan_LIBRARY NAMES vulkan HINTS $ENV{VULKAN_SDK}/lib REQUIRED)
        IF (Vulkan_LIBRARY)
            set(Vulkan_FOUND ON)
            include_directories($ENV{VULKAN_SDK}/include)
            MESSAGE("Using bundled Vulkan library version")
        ENDIF()
        set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XLIB_KHR -DVK_USE_PLATFORM_XCB_KHR")
    ELSE()
        MESSAGE(FATAL_ERROR "Vulkan not found. Make sure you have Vulkan SDK installed in your system")
    ENDIF()
    find_package(Threads REQUIRED)
ELSEIF(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_MACOS_MVK -DVK_EXAMPLE_XCODE_GENERATED")
    # Todo : android?
ENDIF(WIN32)

IF (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Could not find Vulkan library!")
ELSE()
    message(STATUS ${Vulkan_LIBRARY})
ENDIF()


set(RENDERER_LIB_DIR ${RENDERER_INSTALL_DIR}/lib)
set(RENDERER_INCLUDE_DIR ${RENDERER_INSTALL_DIR}/include)
include_directories(${RENDERER_INCLUDE_DIR})
include_directories(${RENDERER_INCLUDE_DIR}/vkw)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../glm)
find_library(RENDERER_LIB NAMES VkWrapper.a libVkWrapper libVkWrapper.so PATHS ${RENDERER_LIB_DIR} REQUIRED)
message(STATUS ${RENDERER_LIB})


find_package(glfw3 3.3 REQUIRED)
set(GLFW_LIBRARY glfw)

add_executable(TestApp main.cpp)
include_directories(source)
add_subdirectory(source)

target_link_libraries(TestApp TEST_OBJECTS -L${RENDERER_LIB_DIR} ${RENDERER_LIB})


include(cmake/compile_shaders.cmake)

compileShaders(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} TestApp .)