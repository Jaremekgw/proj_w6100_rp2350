set(TARGET_NAME kitchen_pwm)

add_executable(${TARGET_NAME})

# set(PIO_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)  # for the generated pio header
# pico_generate_pio_header(${TARGET_NAME}
#     ${CMAKE_CURRENT_LIST_DIR}/ws2815.pio
#     OUTPUT_DIR ${PIO_OUTPUT_DIR}
# )

target_sources(${TARGET_NAME} PRIVATE
        main.c
        network.c
        wizchip_custom.c
        telnet.c
        partition.c
        efu_update.c
        utility.c
        flash_cfg.c
        vl53l8cx_drv.c
        vl53l8cx_api.c
        vl53l8cx_platform.c
        vl53_diag.c
        pwm_api.c
        pwm_drv.c
        rd03d_drv.c
        rd03d_api.c
        rd03d_cli.c
        )

target_include_directories(${TARGET_NAME} PRIVATE
        ${PIO_OUTPUT_DIR}                       # see: # ${CMAKE_CURRENT_BINARY_DIR}/generated
        # ${CMAKE_CURRENT_LIST_DIR}               # for local headers
        # ${CMAKE_CURRENT_SOURCE_DIR}
        # ${CMAKE_CURRENT_BINARY_DIR}
        )

# and make sure the folder exists but ignored by Git
file(MAKE_DIRECTORY ${PIO_OUTPUT_DIR})

# pull in common dependencies and additional spi hardware support
target_link_libraries(${TARGET_NAME} PRIVATE
        pico_stdlib
        hardware_spi
        hardware_i2c
        hardware_pwm
        hardware_dma            # added for DMA support
        hardware_pio            # added for PIO support
        ETHERNET_FILES          # libraries/CMakeLists.txt:2:add_library(ETHERNET_FILES STATIC)
        IOLIBRARY_FILES         # port/CMakeLists.txt:2:add_library(IOLIBRARY_FILES STATIC)
        LOOPBACK_FILES          # libraries/CMakeLists.txt:110:add_library(LOOPBACK_FILES STATIC)
        pico_bootrom
        hardware_flash
        boot_uf2_headers
        hardware_clocks         # ??? for setting clock speeds
        )

add_custom_target(show_pio_headers_${TARGET_NAME}
    COMMAND ${CMAKE_COMMAND} -E echo "Headers generated in: ${CMAKE_CURRENT_BINARY_DIR}/generated"
)

pico_embed_pt_in_binary(${TARGET_NAME} ${CMAKE_CURRENT_LIST_DIR}/../w6100_partitions.json)

pico_enable_stdio_usb(${TARGET_NAME} 1)
pico_enable_stdio_uart(${TARGET_NAME} 0)        # to use Debug Probe via UART, used by RD03D


# create map/bin/hex file etc.
pico_add_extra_outputs(${TARGET_NAME})          # this function delares that you want a UF2 file to be generated

