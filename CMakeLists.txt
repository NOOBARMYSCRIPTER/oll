cmake_minimum_required(VERSION 3.10)
project(oxide_bypass)

set(CMAKE_CXX_STANDARD 11)

set(Plugin.ClosureTrampoline OFF CACHE INTERNAL "Disable ClosureTrampoline module")
set(DOBBY_GENERATE_SHARED OFF CACHE INTERNAL "Build Dobby as static library")

add_subdirectory(dobby)

if(ANDROID)
    target_compile_options(dobby PRIVATE "-Dcommon_closure_bridge_handler@PAGE=:pg_hi21:common_closure_bridge_handler")
    target_compile_options(dobby PRIVATE "-Dcommon_closure_bridge_handler@PAGEOFF=:lo12:common_closure_bridge_handler")
endif()

add_library(oxide_bypass SHARED main.cpp)

target_link_libraries(oxide_bypass dobby log)

target_include_directories(oxide_bypass PRIVATE dobby/include)
