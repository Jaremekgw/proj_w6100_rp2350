
# 1. layout for library WIZnet-PICO-C
pico2/projects/
│
├── CMakeLists.txt         ← your main build
│
└── libraries/             ← all vendor files in one place
    │
    ├── CMakeLists.txt     ← copied from WIZnet-PICO-C/libraries/CMakeLists.txt
    │
    ├── ioLibrary_Driver/  ← copied from WIZnet-PICO-C/libraries/ioLibrary_Driver
    │     └── ... sources ...
    │
    ├── mbedtls/           ← copied from WIZnet-PICO-C/libraries/mbedtls
    │     └── ... sources ...
    │
    └── port/              ← copied from WIZnet-PICO-C/port
          └── ... sources ...
```copy
  $ mkdir libraries
  $ cp ~/project/pico2/work/WIZnet-PICO-C/libraries/CMakeLists.txt libraries/
  $ cp -r ~/project/pico2/work/WIZnet-PICO-C/libraries/ioLibrary_Driver/ libraries/
  $ cp -r ~/project/pico2/work/WIZnet-PICO-C/libraries/mbedtls/ libraries/
  $ cp -r ~/project/pico2/work/WIZnet-PICO-C/port/ libraries/

```


# 2. How to integrate port/ into libraries/CMakeLists.txt
- in libraries/CMakeLists.txt add/modify following:
# ioLibrary_Driver build
add_subdirectory(ioLibrary_Driver)
# mbedtls build
add_subdirectory(mbedtls)
# port build
add_subdirectory(port)

- in root CMakeLists.txt add/modify following:
add_subdirectory(libraries)
target_link_libraries(my_project
    pico_stdlib
    wizchip          # created by libraries/CMakeLists
    mbedtls          # created by mbedtls/CMakeLists
)


# 3. Prepare partitions
- partitions are defined in json file: w6100_partitions.json

- after partition are defined convert to uf2 format:
  $ sudo picotool partition create w6100_partitions.json w6100_part.uf2
  $ sudo picotool partition create w6100_partitions_v2.json w6100_part2.uf2

- load new partition table into board
  $ sudo picotool load w6100_part2.uf2


# 4. Other options for picotool
  $ sudo picotool info -a
  $ sudo picotool erase
  $ sudo picotool erase -r 10000000 10002000
Erasing:              [==============================]  100%
Erased 8192 bytes

# 5. Add pwm to drive RGBW leds
  drivers/
  ├── pwm_rgbw_drv.h     // low-level PWM / hardware binding
  ├── pwm_rgbw_drv.c
  ├── pwm_rgbw_api.h     // high-level color + brightness API
  └── pwm_rgbw_api.c

