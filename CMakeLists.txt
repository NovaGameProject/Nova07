cmake_minimum_required(VERSION 3.20)

project(Nova07 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find packages (via vcpkg)
find_package(SDL3 CONFIG REQUIRED)
find_package(reflectcpp CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(unofficial-luau CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_path(LUABRIDGE3_INCLUDE_DIRS "luabridge3/LuaBridge/Array.h")


# Collect all Source Files automatically
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.hpp"
    "src/*.h"
)

# Create the Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include Directories
target_include_directories(${PROJECT_NAME} PUBLIC
    "src"
    ${SDL3_INCLUDE_DIRS}
    ${LUABRIDGE3_INCLUDE_DIRS}
)


target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    reflectcpp::reflectcpp
    glm::glm
    Jolt::Jolt
    pugixml::static
)
