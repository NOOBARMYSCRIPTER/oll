cmake_minimum_required(VERSION 3.10)
project(oxide_bypass)

set(CMAKE_CXX_STANDARD 11)

set(Plugin.ClosureTrampoline OFF CACHE INTERNAL "Disable ClosureTrampoline module")

add_subdirectory(dobby)

add_library(oxide_bypass SHARED main.cpp)

target_link_libraries(oxide_bypass dobby log)

target_include_directories(oxide_bypass PRIVATE dobby/include)
