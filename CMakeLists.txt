cmake_minimum_required(VERSION 3.22)
project(OpenStreetMapDrive VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(Vulkan REQUIRED)

# Some development machines have stale Vulkan headers in /usr/local/include that
# shadow distro headers. Put a generated header shim first when system headers
# are available so builds remain reproducible without modifying the machine.
set(OSM_DRIVE_VULKAN_HEADER_SHIM "${CMAKE_BINARY_DIR}/generated_vulkan_headers")
if(EXISTS "/usr/include/vulkan/vulkan.h")
  file(MAKE_DIRECTORY "${OSM_DRIVE_VULKAN_HEADER_SHIM}")
  if(NOT EXISTS "${OSM_DRIVE_VULKAN_HEADER_SHIM}/vulkan")
    file(CREATE_LINK "/usr/include/vulkan" "${OSM_DRIVE_VULKAN_HEADER_SHIM}/vulkan" SYMBOLIC)
  endif()
  if(EXISTS "/usr/include/vk_video" AND NOT EXISTS "${OSM_DRIVE_VULKAN_HEADER_SHIM}/vk_video")
    file(CREATE_LINK "/usr/include/vk_video" "${OSM_DRIVE_VULKAN_HEADER_SHIM}/vk_video" SYMBOLIC)
  endif()
endif()

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)

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

find_program(GLSLC_EXECUTABLE glslc)
find_program(GLSLANG_VALIDATOR_EXECUTABLE glslangValidator)
set(OSM_DRIVE_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
set(OSM_DRIVE_SHADER_SPV
  "${OSM_DRIVE_SHADER_OUTPUT_DIR}/road.vert.spv"
  "${OSM_DRIVE_SHADER_OUTPUT_DIR}/road.frag.spv"
  "${OSM_DRIVE_SHADER_OUTPUT_DIR}/terrain.frag.spv"
  "${OSM_DRIVE_SHADER_OUTPUT_DIR}/car.vert.spv"
  "${OSM_DRIVE_SHADER_OUTPUT_DIR}/car.frag.spv"
)
if(GLSLC_EXECUTABLE OR GLSLANG_VALIDATOR_EXECUTABLE)
  file(MAKE_DIRECTORY "${OSM_DRIVE_SHADER_OUTPUT_DIR}")

  if(GLSLC_EXECUTABLE)
    set(OSM_DRIVE_SHADER_COMPILER ${GLSLC_EXECUTABLE})
    set(OSM_DRIVE_SHADER_COMPILER_ARGS)
  else()
    set(OSM_DRIVE_SHADER_COMPILER ${GLSLANG_VALIDATOR_EXECUTABLE})
    set(OSM_DRIVE_SHADER_COMPILER_ARGS -V)
  endif()

  add_custom_command(
    OUTPUT "${OSM_DRIVE_SHADER_OUTPUT_DIR}/road.vert.spv"
    COMMAND ${OSM_DRIVE_SHADER_COMPILER} ${OSM_DRIVE_SHADER_COMPILER_ARGS} "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/road.vert" -o "${OSM_DRIVE_SHADER_OUTPUT_DIR}/road.vert.spv"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/road.vert"
    VERBATIM
  )
  add_custom_command(
    OUTPUT "${OSM_DRIVE_SHADER_OUTPUT_DIR}/road.frag.spv"
    COMMAND ${OSM_DRIVE_SHADER_COMPILER} ${OSM_DRIVE_SHADER_COMPILER_ARGS} "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/road.frag" -o "${OSM_DRIVE_SHADER_OUTPUT_DIR}/road.frag.spv"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/road.frag"
    VERBATIM
  )
  add_custom_command(
    OUTPUT "${OSM_DRIVE_SHADER_OUTPUT_DIR}/terrain.frag.spv"
    COMMAND ${OSM_DRIVE_SHADER_COMPILER} ${OSM_DRIVE_SHADER_COMPILER_ARGS} "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/terrain.frag" -o "${OSM_DRIVE_SHADER_OUTPUT_DIR}/terrain.frag.spv"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/terrain.frag"
    VERBATIM
  )
  add_custom_command(
    OUTPUT "${OSM_DRIVE_SHADER_OUTPUT_DIR}/car.vert.spv"
    COMMAND ${OSM_DRIVE_SHADER_COMPILER} ${OSM_DRIVE_SHADER_COMPILER_ARGS} "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/car.vert" -o "${OSM_DRIVE_SHADER_OUTPUT_DIR}/car.vert.spv"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/car.vert"
    VERBATIM
  )
  add_custom_command(
    OUTPUT "${OSM_DRIVE_SHADER_OUTPUT_DIR}/car.frag.spv"
    COMMAND ${OSM_DRIVE_SHADER_COMPILER} ${OSM_DRIVE_SHADER_COMPILER_ARGS} "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/car.frag" -o "${OSM_DRIVE_SHADER_OUTPUT_DIR}/car.frag.spv"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/car.frag"
    VERBATIM
  )
  add_custom_target(osm_drive_shaders DEPENDS ${OSM_DRIVE_SHADER_SPV})
else()
  message(WARNING "No GLSL-to-SPIR-V compiler found; building without compiled road shaders. Install glslc, glslang-tools, or Vulkan SDK shader tools for mesh rendering.")
endif()

add_executable(osm_drive ${OSM_DRIVE_SOURCES})
target_include_directories(osm_drive BEFORE PRIVATE src "${OSM_DRIVE_VULKAN_HEADER_SHIM}")
target_link_libraries(osm_drive PRIVATE Vulkan::Vulkan glfw glm::glm tinyxml2)
if(TARGET osm_drive_shaders)
  add_dependencies(osm_drive osm_drive_shaders)
endif()

target_compile_options(osm_drive PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

target_compile_definitions(osm_drive PRIVATE
  OSM_DRIVE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
  OSM_DRIVE_SHADER_DIR="${OSM_DRIVE_SHADER_OUTPUT_DIR}"
  GLM_FORCE_DEPTH_ZERO_TO_ONE
)
