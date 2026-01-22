cmake_minimum_required(VERSION 3.20)
project(FlareEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

enable_testing()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/engine/CMakeLists.txt")
	add_subdirectory(engine)
endif()
