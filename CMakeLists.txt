cmake_minimum_required(VERSION 3.10)
project(oxide_bypass)

set(CMAKE_CXX_STANDARD 11)

set(DOBBY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dobby_prebuilt)

add_library(dobby STATIC IMPORTED)
set_target_properties(dobby PROPERTIES
    IMPORTED_LOCATION "${DOBBY_DIR}/lib/arm64-v8a/libdobby.a"
)

add_library(oxide_bypass SHARED main.cpp)

target_include_directories(oxide_bypass PRIVATE "${DOBBY_DIR}/include")

target_link_libraries(oxide_bypass dobby log)
