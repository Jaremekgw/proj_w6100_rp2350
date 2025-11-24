# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# CMake minimum required version
cmake_minimum_required(VERSION 3.12)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BOARD_NAME W6100_EVB_PICO2)

set(PICO_PLATFORM rp2350)
set(WIZNET_CHIP W6100)
add_definitions(-D_WIZCHIP_=W6100)
add_definitions(-DDEVICE_BOARD_NAME=W6100_EVB_PICO2)



# if(NOT DEFINED PICO_SDK_PATH)
    # set(PICO_SDK_PATH ${CMAKE_SOURCE_DIR}/../work/WIZnet-PICO-C/libraries/pico-sdk)
    # set(PICO_SDK_PATH ${CMAKE_SOURCE_DIR}/../pico-sdk)
#    message(STATUS "PICO_SDK_PATH = ${PICO_SDK_PATH}")
# endif()
message(STATUS "PICO_SDK_PATH = ${PICO_SDK_PATH}")

include(pico_sdk_import.cmake)
include(wiznet_pico_c_sdk_version.cmake)

# Set project name
set(PROJECT_NAME projects_2350)

# Set project informations
project(${PROJECT_NAME} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_DEOPTIMIZED_DEBUG 1)   # I added this

# Initialize the SDK
pico_sdk_init()


# Set the project root directory if it's not already defined, as may happen if
# the tests folder is included directly by a parent project, without including
# the top level CMakeLists.txt.
if(NOT DEFINED WIZNET_DIR)
    set(WIZNET_DIR ${CMAKE_SOURCE_DIR}/../work/WIZnet-PICO-C/libraries/ioLibrary_Driver)
    message(STATUS "WIZNET_DIR = ${WIZNET_DIR}")
endif()

if(NOT DEFINED MBEDTLS_DIR)
    set(MBEDTLS_DIR ${CMAKE_SOURCE_DIR}/../work/WIZnet-PICO-C/libraries/mbedtls)
    message(STATUS "MBEDTLS_DIR = ${MBEDTLS_DIR}")
endif()

if(NOT DEFINED PORT_DIR)
    set(PORT_DIR ${CMAKE_SOURCE_DIR}/../work/WIZnet-PICO-C/port)
    message(STATUS "PORT_DIR = ${PORT_DIR}")
endif()

# Turn off mbedtls test mode 
set(ENABLE_PROGRAMS OFF CACHE BOOL "Build mbedtls programs")
set(ENABLE_TESTING OFF CACHE BOOL "Build mbedtls testing")
add_definitions(-DMBEDTLS_CONFIG_FILE="${PORT_DIR}/mbedtls/inc/ssl_config.h")
add_definitions(-DSET_TRUSTED_CERT_IN_SAMPLES)
add_definitions(-DPICO_USE_FASTEST_SUPPORTED_CLOCK=1)

# Set compile options
add_compile_options(
        -Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )

# Turn off CMake deprecation warnings for subprojects
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Disable deprecated warnings")

# Add libraries in subdirectories
add_subdirectory(
    ${CMAKE_SOURCE_DIR}/../work/WIZnet-PICO-C/libraries
    ${CMAKE_BINARY_DIR}/WIZnet-PICO-C/libraries
)
add_subdirectory(
    ${MBEDTLS_DIR}
    ${CMAKE_BINARY_DIR}/mbedtls
)
add_subdirectory(
    ${PORT_DIR}
    ${CMAKE_BINARY_DIR}/port
)


# Add your projects
add_subdirectory(stairs_ws2815)
add_subdirectory(scd30_meter)
add_subdirectory(layout_seeder)

# Add a top-level “meta-target”
# Create a meta target that builds both projects
add_custom_target(all_projects
        DEPENDS
        proj_stairs_ws2815
        proj_scd30_meter
)
