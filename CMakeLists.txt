cmake_minimum_required(VERSION 3.18)
project(ifd_sscp_driver C)

include(FetchContent)
include(GNUInstallDirs)
set(CMAKE_C_STANDARD 99)

find_package(Threads REQUIRED)

set(LIB_NAME ifd-sscp)

set(SSCP_HOST_GIT_REPOSITORY "https://github.com/springcard/sscp-host.git" CACHE STRING "sscp-host Git repository")
set(SSCP_HOST_GIT_TAG "master" CACHE STRING "sscp-host Git tag, branch, or commit")
set(SSCP_HOST_SOURCE_DIR "${CMAKE_BINARY_DIR}/sscp-host" CACHE PATH "sscp-host checkout directory")

FetchContent_Declare(sscp_host
    GIT_REPOSITORY ${SSCP_HOST_GIT_REPOSITORY}
    GIT_TAG ${SSCP_HOST_GIT_TAG}
    GIT_PROGRESS TRUE
    SOURCE_DIR ${SSCP_HOST_SOURCE_DIR}
    # Only fetch the checkout; build the library locally without upstream examples.
    SOURCE_SUBDIR cmake-do-not-add
)

FetchContent_MakeAvailable(sscp_host)

file(GLOB SSCP_HOST_SOURCES "${sscp_host_SOURCE_DIR}/src/*.c")

if(WIN32)
    list(REMOVE_ITEM SSCP_HOST_SOURCES "${sscp_host_SOURCE_DIR}/src/sscp-host-serial-linux.c")
else()
    list(REMOVE_ITEM SSCP_HOST_SOURCES "${sscp_host_SOURCE_DIR}/src/sscp-host-serial-windows.c")
endif()

add_library(sscp-host STATIC ${SSCP_HOST_SOURCES})
target_include_directories(sscp-host PUBLIC "${sscp_host_SOURCE_DIR}/inc")
set_target_properties(sscp-host PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(${LIB_NAME} SHARED ifd-sscp.c ifd-sscp-data.c ifd-sscp-bridge.c ifd-sscp-ipc.c ifd-sscp-utils.c ifd-sscp-control.c ifd-sscp-debug.c)
set_target_properties(${LIB_NAME} PROPERTIES OUTPUT_NAME "ifd-sscp")

target_include_directories(${LIB_NAME} PRIVATE /usr/include/PCSC)
target_link_libraries(${LIB_NAME} PRIVATE sscp-host Threads::Threads)

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
