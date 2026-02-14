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

# current status: (from wiznet_pico_c_sdk_version.cmake)
# -- WIZNET-PICO-C SDK version is 2.2.0

# if(NOT DEFINED PICO_SDK_PATH)
    # set(PICO_SDK_PATH ${CMAKE_SOURCE_DIR}/../work/WIZnet-PICO-C/libraries/pico-sdk)
    # set(PICO_SDK_PATH ${CMAKE_SOURCE_DIR}/../pico-sdk)
#    message(STATUS "PICO_SDK_PATH = ${PICO_SDK_PATH}")
# endif()
message(STATUS "PICO_SDK_PATH = ${PICO_SDK_PATH}")
# current status:
# -- PICO_SDK_PATH = /home/jaremekg/.pico-sdk/sdk/2.2.0

# include(pico_sdk_import.cmake)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(wiznet_pico_c_sdk_version.cmake)

# Tell CMake this is a bare-metal toolchain
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

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
    set(WIZNET_DIR ${CMAKE_SOURCE_DIR}/libraries/ioLibrary_Driver)
    message(STATUS "WIZNET_DIR = ${WIZNET_DIR}")
endif()

if(NOT DEFINED MBEDTLS_DIR)
    set(MBEDTLS_DIR ${CMAKE_SOURCE_DIR}/libraries/mbedtls)
    message(STATUS "MBEDTLS_DIR = ${MBEDTLS_DIR}")
endif()

if(NOT DEFINED PORT_DIR)
    set(PORT_DIR ${CMAKE_SOURCE_DIR}/libraries/port)
    message(STATUS "PORT_DIR = ${PORT_DIR}")
endif()

# Turn off mbedtls test mode 
set(ENABLE_PROGRAMS OFF CACHE BOOL "Build mbedtls programs")
set(ENABLE_TESTING OFF CACHE BOOL "Build mbedtls testing")
add_definitions(-DMBEDTLS_CONFIG_FILE="${PORT_DIR}/mbedtls/inc/ssl_config.h")
add_definitions(-DSET_TRUSTED_CERT_IN_SAMPLES)
add_definitions(-DPICO_USE_FASTEST_SUPPORTED_CLOCK=1)

# # Set compile options
# add_compile_options(
#         -fdiagnostics-show-location=once
#         $<$<COMPILE_LANGUAGE:C>:-Wno-sign-conversion>
#         )

#  Create a dedicated warning interface library (see: target_link_libraries)
add_library(project_warnings INTERFACE)

target_compile_options(project_warnings INTERFACE
    -Wall
    -Wextra
    -Wconversion
    -Wsign-conversion
    -Wshadow
    # -Wformat
    -Wno-format
    # -Wno-unused-function    # suppress: libraries/port/ioLibrary_Driver/inc/wizchip_spi.h:146:13: warning: 'wizchip_critical_section_lock' declared 'static' but never defined [-Wunused-function]
)

# Turn off CMake deprecation warnings for subprojects
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Disable deprecated warnings")

# # Add copied libraries from WIZnet-PICO-C - OPTION 3
add_subdirectory(
    ${CMAKE_SOURCE_DIR}/libraries
    ${CMAKE_BINARY_DIR}/wiznet
)

# Add your projects
add_subdirectory(stairs_ws2815)
# add_subdirectory(scd30_meter)
add_subdirectory(tree_ws2815)
add_subdirectory(kitchen_pwm)

# Add a top-level “meta-target”
# Create a meta target that builds both projects
add_custom_target(all_projects
        DEPENDS
        # proj_stairs_ws2815
        # proj_scd30_meter
        kitchen_pwm
)
