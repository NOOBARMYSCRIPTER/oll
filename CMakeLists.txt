cmake_minimum_required(VERSION 3.10)
project(oxide_bypass)

set(CMAKE_CXX_STANDARD 11)

set(GENERATE_SHARED OFF CACHE INTERNAL "")
set(Dobby.Plugin.ClosureTrampoline OFF CACHE INTERNAL "")
set(Plugin.ClosureTrampoline OFF CACHE INTERNAL "")

set(DOBBY_BUILD_TESTS OFF CACHE INTERNAL "")

add_subdirectory(dobby)

add_library(oxide_bypass SHARED main.cpp)

target_link_libraries(oxide_bypass dobby log)

target_include_directories(oxide_bypass PRIVATE dobby/include)
