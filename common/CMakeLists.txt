set(COMMON_LIB common)

add_library(${COMMON_LIB} STATIC
    board/partition.c
    utils/utility.c
    efu/efu_update.c
    wiznet/wizchip_custom.c
)

target_include_directories(${COMMON_LIB}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/board
        ${CMAKE_CURRENT_LIST_DIR}/utils
        ${CMAKE_CURRENT_LIST_DIR}/efu
        ${CMAKE_CURRENT_LIST_DIR}/wiznet
)
# you can set         ${CMAKE_CURRENT_LIST_DIR}/board  and then include "partition.h" in your source files, or just         ${CMAKE_CURRENT_LIST_DIR} and then include "board/partition.h" in your source files, whatever you prefer. The important thing is that the directory containing the header file is included in the target_include_directories for the common library, and that the common library is linked to any targets that need to use the partition functions.
# or just         ${CMAKE_CURRENT_LIST_DIR} and then include "board/partition.h" in your source files

# must be PUBLIC to propagate the include directories to the targets that link against common
target_link_libraries(${COMMON_LIB} PUBLIC
    pico_stdlib
    pico_bootrom
    hardware_flash
    boot_uf2_headers

    ETHERNET_FILES
    IOLIBRARY_FILES
)

enable_strict_warnings(${COMMON_LIB})

set_source_files_properties(
    efu/efu_update.c
    wiznet/wizchip_custom.c
    PROPERTIES COMPILE_OPTIONS -Wno-unused-function
)

