cmake_minimum_required(VERSION 3.10)
project(oxide_bypass)

set(CMAKE_CXX_STANDARD 11)

set(ANDROID_ABI "x86_64")
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 24)

set(DOBBY_PREBUILT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dobby_prebuilt)

set(DOBBY_STATIC_LIB "${DOBBY_PREBUILT_DIR}/build/android/x86_64/libdobby.a")

file(GLOB_RECURSE DOBBY_H_FILE "${DOBBY_PREBUILT_DIR}/*dobby.h")
if(DOBBY_H_FILE)
    list(GET DOBBY_H_FILE 0 FIRST_MATCH)
    get_filename_component(DOBBY_INCLUDE_DIR "${FIRST_MATCH}" DIRECTORY)
    message(STATUS "[+] Found Dobby include directory: ${DOBBY_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "[-] dobby.h not found in unpacked archive!")
endif()

add_library(dobby STATIC IMPORTED)
set_target_properties(dobby PROPERTIES
    IMPORTED_LOCATION "${DOBBY_STATIC_LIB}"
)

add_library(oxide_bypass SHARED main.cpp)

target_include_directories(oxide_bypass PRIVATE "${DOBBY_INCLUDE_DIR}")

target_link_libraries(oxide_bypass dobby log)
