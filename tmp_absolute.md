$ sudo picotool info -a build/layout_seeder/w6100_part_layout.uf2
File build/layout_seeder/w6100_part_layout.uf2 family ID 'absolute':

Program Information
 name:                  w6100_part_layout
 binary start:          0x10000000
 binary end:            0x10003448
 target chip:           RP2350
 image type:            ARM Secure

Fixed Pin Information
 none

Build Information
 sdk version:           2.2.1-develop
 pico_board:            pico2
 boot2_name:            boot2_w25q080
 build date:            Nov 17 2025
 build attributes:      All optimization disabled
                        Debug

Metadata Block 1
 address:               0x10000138
 next block address:    0x10003448
 block type:            image def
 target chip:           RP2350
 image type:            ARM Secure

Metadata Block 2
 address:               0x10003448
 next block address:    0x100034dc
 block type:            partition table
 partition table:       non-singleton
 un-partitioned space:  S(rw) NSBOOT(rw) NS(rw), uf2 { 'absolute' }
 partition 0 (A):       00002000->000fc000 S(rw) NSBOOT(rw) NS(rw), id=0000000000000000, "Main A", uf2 { 'rp2350-arm-s', 'rp2350-riscv' }, arm_boot 1, riscv_boot 1
 partition 1 (B w/ 0):  000fc000->001f6000 S(rw) NSBOOT(rw) NS(rw), id=0000000000000001, "Main B", uf2 { 'rp2350-arm-s', 'rp2350-riscv' }, arm_boot 1, riscv_boot 1
 partition 2 (A):       001f6000->001fe000 S(rw) NSBOOT(rw) NS(rw), id=0000000000000002, "Config", uf2 { 'data' }, arm_boot 1, riscv_boot 1
 version:               1.0

Metadata Block 3
 address:               0x100034dc
 next block address:    0x10000138
 block type:            image def
 target chip:           RP2350
 image type:            ARM Secure
 load map entry 0:      Load 0x10000000->0x10002f2c
 load map entry 1:      Copy 0x10002f2c->0x10003448 to 0x20000110->0x2000062c
 load map entry 2:      Load 0x10003448->0x100034dc

