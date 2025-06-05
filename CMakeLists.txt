cmake_minimum_required(VERSION 3.10)
project(ifd_sscp_driver C)

include(GNUInstallDirs)
set(CMAKE_C_STANDARD 99)

find_package(Threads REQUIRED)

set(LIB_NAME ifd-sscp)

include_directories(/usr/include/PCSC)
include_directories(${PROJECT_SOURCE_DIR}/sscp-host/inc)

file(GLOB SSCP_HOST_SOURCES "${PROJECT_SOURCE_DIR}/sscp-host/src/*.c")

add_library(${LIB_NAME} SHARED ifd-sscp.c ifd-sscp-bridge.c ifd-sscp-ipc.c ${SSCP_HOST_SOURCES})
set_target_properties(${LIB_NAME} PROPERTIES OUTPUT_NAME "ifd-sscp")

target_link_libraries(${LIB_NAME} PRIVATE Threads::Threads)

add_custom_command(TARGET ${LIB_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            ${CMAKE_BINARY_DIR}/bundle/ifd-sscp.bundle/Contents/Linux
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/Info.plist
            ${CMAKE_BINARY_DIR}/bundle/ifd-sscp.bundle/Contents/Info.plist
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${LIB_NAME}>
            ${CMAKE_BINARY_DIR}/bundle/ifd-sscp.bundle/Contents/Linux/
)

# Forcer l'installation dans /usr/lib ou /usr/lib64
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LIB_SUBDIR lib64)
else()
    set(LIB_SUBDIR lib)
endif()

install(DIRECTORY ${CMAKE_BINARY_DIR}/bundle/ifd-sscp.bundle
        DESTINATION /usr/lib/pcsc/drivers)
