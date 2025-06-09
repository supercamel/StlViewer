cmake_minimum_required(VERSION 3.15)
project(stlviewer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add dependencies
add_subdirectory(external/bgfx.cmake)  # https://github.com/bkaradzic/bgfx.cmake
find_package(assimp REQUIRED)
find_package(glfw3 REQUIRED)

# Shader output directory
set(BGFX_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${BGFX_SHADER_DIR})

# Source files
set(SRC
    main.cpp
    mesh_loader.cpp
    camera.cpp
    shader_utils.cpp
)

add_executable(stlviewer ${SRC})

target_include_directories(stlviewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx.cmake/bgfx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx.cmake/bx/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/bgfx.cmake/bimg/include
)

target_link_libraries(stlviewer PRIVATE
    bgfx
    bimg
    bx
    assimp
    glfw
)
