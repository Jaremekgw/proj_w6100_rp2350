set(TARGET_NAME ceiling_ms)

add_executable(${TARGET_NAME})

target_sources(${TARGET_NAME} PRIVATE
        main.c
        telnet.c
        vl53l8cx_drv.c
        vl53l8cx_api.c
        vl53l8cx_platform.c
        vl53_diag.c
        pwm_api.c
        pwm_drv.c
        ms72sf1_drv.c
        ms72sf1_parser.c
        ms72sf1_api.c
        ms72sf1_cli.c
)

# set_source_files_properties(
#     main.c
#     PROPERTIES COMPILE_OPTIONS -Wno-unused-function
# )

# pull in common dependencies and additional spi hardware support
target_link_libraries(${TARGET_NAME} PRIVATE
        common
        pico_stdlib
        # hardware_spi
        hardware_i2c            # required by the retained VL53L8CX sources
        hardware_uart
        hardware_pwm
        hardware_dma            # added for DMA support
        hardware_pio            # added for PIO support
        # IOLIBRARY_FILES         # port/CMakeLists.txt:2:add_library(IOLIBRARY_FILES STATIC)
        # LOOPBACK_FILES          # libraries/CMakeLists.txt:110:add_library(LOOPBACK_FILES STATIC)
        # ETHERNET_FILES          # libraries/CMakeLists.txt:2:add_library(ETHERNET_FILES STATIC)

        # hardware_clocks         # ??? for setting clock speeds
)

# call function from root CMakeLists.txt to set strict warning flags for this target
enable_strict_warnings(${TARGET_NAME})

# Then suppress SDK warnings properly:
target_compile_options(pico_stdlib INTERFACE -Wno-sign-conversion)
# And vendor:
target_compile_options(IOLIBRARY_FILES PRIVATE -Wno-unused-function)

pico_embed_pt_in_binary(${TARGET_NAME} ${CMAKE_CURRENT_LIST_DIR}/../w6100_partitions.json)

pico_enable_stdio_usb(${TARGET_NAME} 1)
pico_enable_stdio_uart(${TARGET_NAME} 0)        # to use Debug Probe via UART, used by MS72SF1


# create map/bin/hex file etc.
pico_add_extra_outputs(${TARGET_NAME})          # this function delares that you want a UF2 file to be generated

