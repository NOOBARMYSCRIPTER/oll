cmake_minimum_required(VERSION 3.10)
project(oxide_bypass)

set(CMAKE_CXX_STANDARD 11)

include(FetchContent)
FetchContent_Declare(
  dobby
  GIT_REPOSITORY https://github.com
  GIT_TAG        master
  SOURCE_DIR     ${CMAKE_CURRENT_BINARY_DIR}/dobby
)

set(Plugin.ClosureTrampoline OFF CACHE INTERNAL "Disable ClosureTrampoline module")

FetchContent_MakeAvailable(dobby)

add_library(oxide_bypass SHARED main.cpp)

target_link_libraries(oxide_bypass dobby log)
target_include_directories(oxide_bypass PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/dobby/include)
