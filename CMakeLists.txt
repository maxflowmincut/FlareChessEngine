cmake_minimum_required(VERSION 3.20)
project(FlareEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/engine/CMakeLists.txt")
	add_subdirectory(engine)
endif()
