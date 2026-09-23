cmake_minimum_required(VERSION 3.24)
project(OpenStreetMapDrive VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(Vulkan REQUIRED)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)

FetchContent_Declare(
  tinyxml2
  GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
  GIT_TAG 10.0.0
)

FetchContent_MakeAvailable(glfw glm tinyxml2)

file(GLOB_RECURSE OSM_DRIVE_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.hpp)

add_executable(osm_drive ${OSM_DRIVE_SOURCES})
target_include_directories(osm_drive PRIVATE src)
target_link_libraries(osm_drive PRIVATE Vulkan::Vulkan glfw glm::glm tinyxml2)

target_compile_options(osm_drive PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

target_compile_definitions(osm_drive PRIVATE
  OSM_DRIVE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
