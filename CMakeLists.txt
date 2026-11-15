cmake_minimum_required(VERSION 3.21)

project(xyzBot VERSION 1.18.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

if(NOT DEFINED ENV{GEODE_SDK})
  message(FATAL_ERROR "Unable to find Geode SDK! Set the GEODE_SDK environment variable.")
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Video rendering goes through the eclipse.ffmpeg-api mod and is compiled in on Android only.
if(ANDROID)
  target_compile_definitions(${PROJECT_NAME} PRIVATE XYZ_FFMPEG_API=1)
else()
  target_compile_definitions(${PROJECT_NAME} PRIVATE XYZ_FFMPEG_API=0)
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})
