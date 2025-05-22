cmake_minimum_required(VERSION 3.14)
project(ImplicitBooleanCSG VERSION 1.0.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set vcpkg toolchain file (if not specified via command line)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
    message(STATUS "Using vcpkg toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Find dependencies
find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

# Use static runtime for Windows
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Source and header files
set(SOURCES
    src/main.cpp
    src/Renderer.cpp
)

set(HEADERS
    include/ImplicitSurfaces.h
    include/Renderer.h
)

# Main executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE include)

# Link dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE GLEW::GLEW glfw)

# Option to use advanced OpenGL features (e.g., compute shaders)
option(USE_ADVANCED_OPENGL "Use advanced OpenGL features" OFF)
if(USE_ADVANCED_OPENGL)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ADVANCED_OPENGL)
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
)

# Installation targets
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Add build guide documentation
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md" DESTINATION docs OPTIONAL)

# Add OpenGL library for Windows
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
endif()

# Add a simple test target
enable_testing()
add_test(NAME ${PROJECT_NAME}_test COMMAND ${PROJECT_NAME} --test)

# Print configuration information
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "Project will be installed to: ${CMAKE_INSTALL_PREFIX}")