cmake_minimum_required(VERSION 3.20)

project(Nova07 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(SDL3_shadercross CONFIG REQUIRED)
find_package(reflectcpp CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(unofficial-luau CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_path(LUABRIDGE3_INCLUDE_DIRS "luabridge3/LuaBridge/Array.h")

find_program(SHADERCROSS_EXE
    NAMES shadercross
    HINTS
        "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/sdl3-shadercross"
        "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/sdl3-shadercross"
        "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/sdl3-shadercross"
)

if(NOT SHADERCROSS_EXE)
    message(FATAL_ERROR "shadercross not found. Make sure sdl3-shadercross is installed via vcpkg.")
endif()
message(STATUS "Found shadercross: ${SHADERCROSS_EXE}")

add_compile_definitions(TRACY_ENABLE)

set(SHADER_SOURCES
    "${CMAKE_SOURCE_DIR}/shaders/base.vert.hlsl"
    "${CMAKE_SOURCE_DIR}/shaders/base.frag.hlsl"
    "${CMAKE_SOURCE_DIR}/shaders/skybox.vert.hlsl"
    "${CMAKE_SOURCE_DIR}/shaders/skybox.frag.hlsl"
)

set(SHADER_OUTPUTS
    "${CMAKE_SOURCE_DIR}/shaders/base.vert.spv"
    "${CMAKE_SOURCE_DIR}/shaders/base.frag.spv"
    "${CMAKE_SOURCE_DIR}/shaders/skybox.vert.spv"
    "${CMAKE_SOURCE_DIR}/shaders/skybox.frag.spv"
)

add_custom_command(
    OUTPUT ${SHADER_OUTPUTS}
    COMMAND ${SHADERCROSS_EXE} "${CMAKE_SOURCE_DIR}/shaders/base.vert.hlsl" -o "${CMAKE_SOURCE_DIR}/shaders/base.vert.spv"
    COMMAND ${SHADERCROSS_EXE} "${CMAKE_SOURCE_DIR}/shaders/base.frag.hlsl" -o "${CMAKE_SOURCE_DIR}/shaders/base.frag.spv"
    COMMAND ${SHADERCROSS_EXE} "${CMAKE_SOURCE_DIR}/shaders/skybox.vert.hlsl" -o "${CMAKE_SOURCE_DIR}/shaders/skybox.vert.spv"
    COMMAND ${SHADERCROSS_EXE} "${CMAKE_SOURCE_DIR}/shaders/skybox.frag.hlsl" -o "${CMAKE_SOURCE_DIR}/shaders/skybox.frag.spv"
    DEPENDS ${SHADER_SOURCES}
    COMMENT "Compiling HLSL shaders to SPIR-V..."
    VERBATIM
)

add_custom_target(Shaders ALL DEPENDS ${SHADER_OUTPUTS})

file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.hpp"
    "src/*.h"
)

add_executable(${PROJECT_NAME} ${SOURCES})
add_dependencies(${PROJECT_NAME} Shaders)

target_include_directories(${PROJECT_NAME} PUBLIC
    "src"
    ${SDL3_INCLUDE_DIRS}
    ${LUABRIDGE3_INCLUDE_DIRS}
)


target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_shadercross::SDL3_shadercross
    reflectcpp::reflectcpp
    glm::glm
    Jolt::Jolt
    pugixml::static
    Tracy::TracyClient
    unofficial::luau::Luau.VM
    unofficial::luau::Luau.Compiler
    unofficial::luau::Luau.Ast
    unofficial::luau::Luau.Common
)
