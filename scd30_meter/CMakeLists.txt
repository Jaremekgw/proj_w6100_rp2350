set(TARGET_NAME proj_scd30_meter)

add_executable(${TARGET_NAME}
        main.c
        )

target_link_libraries(${TARGET_NAME}
        pico_stdlib
        hardware_spi
        hardware_dma
        hardware_pio
        ETHERNET_FILES
        IOLIBRARY_FILES
        PORT_COMMON_FILES
        LOOPBACK_FILES
        TIMER_FILES
        )

add_definitions(-DCMAKE_BUILD_TYPE=Debug)

target_compile_definitions(${TARGET_NAME} PRIVATE PICO_STDIO_ENABLE_DEBUG=1)

target_include_directories(${TARGET_NAME} SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/libraries
)

target_compile_options(${TARGET_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-format
    -Wno-unused-function
    -Wno-maybe-uninitialized
)

pico_enable_stdio_usb(${TARGET_NAME} 0)
pico_enable_stdio_uart(${TARGET_NAME} 1)

pico_add_extra_outputs(${TARGET_NAME})
