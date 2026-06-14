cmake_minimum_required(VERSION 3.10)
project(oxide_bypass)

set(CMAKE_CXX_STANDARD 11)

set(DOBBY_PREBUILT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dobby_prebuilt)

file(GLOB_RECURSE DOBBY_H_FILE "${DOBBY_PREBUILT_DIR}/*dobby.h")
if(DOBBY_H_FILE)
    list(GET DOBBY_H_FILE 0 FIRST_MATCH)
    get_filename_component(DOBBY_INCLUDE_DIR "${FIRST_MATCH}" DIRECTORY)
    message(STATUS "[+] Found Dobby include directory: ${DOBBY_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "[-] dobby.h not found in unpacked archive!")
endif()

file(GLOB_RECURSE DOBBY_A_FILE "${DOBBY_PREBUILT_DIR}/*libdobby.a")
if(DOBBY_A_FILE)
    list(GET DOBBY_A_FILE 0 DOBBY_STATIC_LIB)
    message(STATUS "[+] Found Dobby static library: ${DOBBY_STATIC_LIB}")
else()
    message(FATAL_ERROR "[-] libdobby.a not found in unpacked archive!")
endif()

add_library(dobby STATIC IMPORTED)
set_target_properties(dobby PROPERTIES
    IMPORTED_LOCATION "${DOBBY_STATIC_LIB}"
)

add_library(oxide_bypass SHARED main.cpp)

target_include_directories(oxide_bypass PRIVATE "${DOBBY_INCLUDE_DIR}")

target_link_libraries(oxide_bypass dobby log)
