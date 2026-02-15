set(COMMON_LIB common)

add_library(${COMMON_LIB} STATIC
    board/partition.c
    utils/utility.c
)

target_include_directories(${COMMON_LIB}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/board
        ${CMAKE_CURRENT_LIST_DIR}/utils
)
# you can set         ${CMAKE_CURRENT_LIST_DIR}/board  and then include "partition.h" in your source files, or just         ${CMAKE_CURRENT_LIST_DIR} and then include "board/partition.h" in your source files, whatever you prefer. The important thing is that the directory containing the header file is included in the target_include_directories for the common library, and that the common library is linked to any targets that need to use the partition functions.
# or just         ${CMAKE_CURRENT_LIST_DIR} and then include "board/partition.h" in your source files

target_link_libraries(${COMMON_LIB} PRIVATE
    pico_stdlib
    pico_bootrom
    hardware_flash
    boot_uf2_headers
)

enable_strict_warnings(${COMMON_LIB})


