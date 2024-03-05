cmake_minimum_required(VERSION 3.2)
project(rtune)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_BUILD_TYPE Debug)

set(RTUNE_VERSION_MAJOR 0)
set(RTUNE_VERSION_MINOR 1)
set(RTUNE_VERSION ${RTUNE_VERSION_MAJOR}.${RTUNE_VERSION_MINOR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/rtune_config.h.cmake "${CMAKE_CURRENT_BINARY_DIR}/src/rtune_config.h" @ONLY)

set(SOURCE_FILES
    src/rtune_runtime.h
    src/rtune_runtime.c
    src/rtune_config.h
)

add_library(rtune SHARED ${SOURCE_FILES})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rtune m)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/src/rtune_config.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rtune_runtime.h
        DESTINATION include)

install(TARGETS rtune
        LIBRARY DESTINATION lib
        )