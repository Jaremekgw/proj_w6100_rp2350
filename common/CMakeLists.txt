set(COMMON_LIB common)

add_library(${COMMON_LIB} STATIC
    board/partition.c
    utils/utility.c
    efu/efu_update.c
    wiznet/wizchip_custom.c
    flash/flash_cfg.c
    network/network.c
    network/tcp_cli.c

)

target_include_directories(${COMMON_LIB}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/board
        ${CMAKE_CURRENT_LIST_DIR}/utils
        ${CMAKE_CURRENT_LIST_DIR}/efu
        ${CMAKE_CURRENT_LIST_DIR}/wiznet
        ${CMAKE_CURRENT_LIST_DIR}/flash
        ${CMAKE_CURRENT_LIST_DIR}/network
)
# you can set         ${CMAKE_CURRENT_LIST_DIR}/board  and then include "partition.h" in your source files, or just         ${CMAKE_CURRENT_LIST_DIR} and then include "board/partition.h" in your source files, whatever you prefer. The important thing is that the directory containing the header file is included in the target_include_directories for the common library, and that the common library is linked to any targets that need to use the partition functions.
# or just         ${CMAKE_CURRENT_LIST_DIR} and then include "board/partition.h" in your source files

# must be PUBLIC to propagate the include directories to the targets that link against common
target_link_libraries(${COMMON_LIB} PUBLIC
    pico_stdlib
    pico_bootrom
    hardware_flash
    boot_uf2_headers

    pico_unique_id

    ETHERNET_FILES
    IOLIBRARY_FILES
    LOOPBACK_FILES
)

enable_strict_warnings(${COMMON_LIB})


# This is a bit of a hack to suppress warnings for unused functions in the VL53L8CX driver, which is a third-party library that we don't want to modify directly. By setting the COMPILE_OPTIONS property for these source files, we can suppress the -Wunused-function warning for this specific target without affecting other targets that might use the same library.
# for network.c see
#          network/network.c   # libraries/port/ioLibrary_Driver/inc/wizchip_spi.h:76:20: warning: 'wizchip_select' declared 'static'
set_source_files_properties(
    # efu/efu_update.c
    wiznet/wizchip_custom.c
    network/network.c
    network/tcp_cli.c
    PROPERTIES COMPILE_OPTIONS -Wno-unused-function
)

