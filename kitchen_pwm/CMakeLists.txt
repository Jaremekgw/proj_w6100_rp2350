set(TARGET_NAME kitchen_pwm)

add_executable(${TARGET_NAME})

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

# Remove this entirely, 
# - You are manually exposing library headers to the executable
# - This bypasses CMake’s dependency model
#target_include_directories(${TARGET_NAME} PRIVATE
#        ${CMAKE_SOURCE_DIR}/libraries/
#        )

# This tells GCC: Treat headers in this directory as system headers. (SYSTEM didn't help)


set_source_files_properties(
    wizchip_custom.c
    network.c
    main.c
    efu_update.c
    telnet.c
    PROPERTIES COMPILE_OPTIONS -Wno-unused-function
)

# This creats problem in clangd with assert.h
# # if you want warning suppression for external libraries
# target_compile_options(${TARGET_NAME} PRIVATE
#         -isystem ${CMAKE_SOURCE_DIR}/libraries/
# )

# pull in common dependencies and additional spi hardware support
target_link_libraries(${TARGET_NAME} PRIVATE
        pico_stdlib
        # pico_unique_id
        hardware_spi
        hardware_i2c
        hardware_pwm
        hardware_dma            # added for DMA support
        hardware_pio            # added for PIO support
        IOLIBRARY_FILES         # port/CMakeLists.txt:2:add_library(IOLIBRARY_FILES STATIC)
        LOOPBACK_FILES          # libraries/CMakeLists.txt:110:add_library(LOOPBACK_FILES STATIC)
        ETHERNET_FILES          # libraries/CMakeLists.txt:2:add_library(ETHERNET_FILES STATIC)
        pico_bootrom
        hardware_flash
        boot_uf2_headers
        hardware_clocks         # ??? for setting clock speeds
)


enable_strict_warnings(${TARGET_NAME})

# Then suppress SDK warnings properly:
target_compile_options(pico_stdlib INTERFACE -Wno-sign-conversion)
# And vendor:
target_compile_options(IOLIBRARY_FILES PRIVATE -Wno-unused-function)

pico_embed_pt_in_binary(${TARGET_NAME} ${CMAKE_CURRENT_LIST_DIR}/../w6100_partitions.json)

pico_enable_stdio_usb(${TARGET_NAME} 1)
pico_enable_stdio_uart(${TARGET_NAME} 0)        # to use Debug Probe via UART, used by RD03D


# create map/bin/hex file etc.
pico_add_extra_outputs(${TARGET_NAME})          # this function delares that you want a UF2 file to be generated

