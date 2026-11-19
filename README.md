# RP2350 + W6100 firmware workspace

This repository contains several independent firmware projects for the Raspberry Pi Pico 2 (RP2350) on the WIZnet `W6100_EVB_PICO2` board. The applications share networking, OTA-update, flash, and diagnostic code in `common/` and vendored WIZnet/mbedTLS code in `libraries/`.

## Repository layout

| Directory | CMake target | Purpose | Build status |
| --- | --- | --- | --- |
| `kitchen_pwm/` | `kitchen_pwm` | RGBW PWM lighting with DDP control, VL53L8CX time-of-flight sensing, and RD03D radar sensing | Enabled |
| `stairs_ws2815/` | `proj_stairs_ws2815` | Parallel PIO/DMA control of WS2815 stair lighting | Enabled |
| `tarrace_ws2815/` | `proj_tarrace_ws2815` | Parallel PIO/DMA control of WS2815 tarrace lighting | Enabled |
| `tree_ws2815/` | `tree_leds` | PIO/DMA control of WS2815 tree lighting with VL53L8CX sensing | Enabled |
| `scd30_meter/` | `proj_scd30_meter` | SCD30 CO2 meter firmware | Disabled in the root `CMakeLists.txt` |
| `layout_seeder/` | `w6100_part_layout` | Utility firmware for embedding/seeding the flash partition layout | Disabled in the root `CMakeLists.txt` |
| `common/` | `common` library | Shared Ethernet, EFU OTA, flash, boot partition, CLI, and diagnostic code | Used by enabled apps |
| `libraries/` | several libraries | Vendored WIZnet ioLibrary, board port, and mbedTLS sources | Used by the workspace |

The `add_subdirectory(...)` entries in the root `CMakeLists.txt` determine which projects are configured. The `all_projects` convenience target currently builds only `kitchen_pwm`.

## IP addresses for project

| Project | IP | Note |
| --- | --- | --- |
| `w6100/tarrace_ws2815/` | 192.168.14.229 | Notice |
| `w6100/kitchen_pwm/` | 192.168.14.228 | Notice |
| `w6100/tree_ws2815/` | 192.168.14.{226,227} | Notice |
| `w6100/stairs_ws2815/` | 192.168.14.225 | Notice |
| `w6100/free4/` | 192.168.14.224 | Notice |
| `w6100/free3/` | 192.168.14.223 | Notice |
| `w6100/free2/` | 192.168.14.222 | Notice |
| `w6100/free1/` | 192.168.14.221 | Notice |
| `pico2w/wash_roof/` | 192.168.14.141 (DHCP) | Notice |


## Requirements

- CMake and Ninja
- Arm toolchain supported by the Pico SDK
- Raspberry Pi Pico SDK 2.2.0 (the `pico` preset currently points to `~/.pico-sdk/sdk/2.2.0`)
- `picotool` 2.2.0 for initial board preparation and USB flashing
- Python 3 for Ethernet firmware upload

## Prepare a board: create the boot partitions

Do this once on a new board, or whenever its partition table must be replaced. The normal layout in `w6100_partitions.json` contains two linked 1000 KiB application slots (`Main A` and `Main B`) for fail-safe OTA updates and a 32 KiB `Config` data partition.

For `w6100_partitions.json` there are 3 partitions

| Partition | id | Family | Size |
| --- | --- | --- | --- |
| `Main A` | 0 | ["rp2350-arm-s", "rp2350-riscv"] | 1 MB |
| `Main B` | 1 | ["rp2350-arm-s", "rp2350-riscv"] | 1 MB |
| `Config` | 2 | ["data"] | 32 kB |

For `w6100_partitions4.json` is additional 'data' partition

| Partition | id | Family | Size |
| --- | --- | --- | --- |
| `Main A` | 0 | ["rp2350-arm-s", "rp2350-riscv"] | 1 MB |
| `Main B` | 1 | ["rp2350-arm-s", "rp2350-riscv"] | 1 MB |
| `Config` | 2 | ["data"] | 8 kB |
| `Data` | 3 | ["data"] | 32 kB |

1. Connect the board over USB in BOOTSEL mode.

   For WSL we have to pass USB connection over Windows to WSL, in PowerShell
   check usb, before:
   ```Powershell
   PS C:\Users\jarek> usbipd list
   Connected:
   BUSID  VID:PID    DEVICE                                  STATE
   1-5    046d:0843  Logitech Webcam C930e                   Not shared
   1-13   26ce:01a2  USB Input Device                        Not shared
   1-14   8087:0033  Intel(R) Wireless Bluetooth(R)          Not shared
   ```
   
   After (press BOOTSEL powering):
   ```Powershell
   PS C:\Users\jarek> usbipd list
   Connected:
   BUSID  VID:PID    DEVICE                                  STATE
   1-1    2e8a:000f  USB Mass Storage Device, RP2350 Boot    Not shared
   1-5    046d:0843  Logitech Webcam C930e                   Not shared
   1-13   26ce:01a2  USB Input Device                        Not shared
   1-14   8087:0033  Intel(R) Wireless Bluetooth(R)          Not shared
   ```

   Use following Commands for usbipd:
   ```Text
   Commands:
     attach   Attach a USB device to a client
     bind     Bind device
   ```

   Run PowerShell as Administrator and run two command with appropriate BUSID:
   ```Powershell
   PS C:\Users\jarek> usbipd bind --busid 1-1
   PS C:\Users\jarek> usbipd attach -a -b 1-1 -w Ubuntu-24.04
   ```

2. From the repository root, convert the partition definition to UF2:

   ```bash
   sudo picotool partition create w6100_partitions.json w6100_part.uf2
   ```

3. Load the partition table and reboot the board:

   ```bash
   sudo picotool load w6100_part.uf2
   sudo picotool reboot
   ```

4. Verify the table and active boot partition:

   ```bash
   sudo picotool info -a
   ```

The alternative `w6100_partitions4.json` adds a separate `Data` partition. Use it only when the firmware is built for that layout. Application targets embed `w6100_partitions.json` in their output with `pico_embed_pt_in_binary`; keep the device table and firmware layout consistent.

After creating the partitions, install an initial application image while the board is connected over USB. For example:

```bash
sudo picotool load -x build/kitchen_pwm/kitchen_pwm.uf2
```

This initial image supplies the bootable firmware and the EFU server required for later network updates.

## Build

Configure the complete CMake tree from the repository root:

```bash
cmake --preset pico
```

Build all enabled targets, or one target by name:

```bash
cmake --build build
cmake --build build --target kitchen_pwm
cmake --build build --target proj_stairs_ws2815
cmake --build build --target tree_leds
```

For a clean rebuild:

```bash
cmake --build build --target clean
cmake --build build
```

Outputs are written below `build/<project>/`, including `.elf`, `.bin`, `.uf2`, and `.map` files. To build a disabled project, first enable its `add_subdirectory(...)` line in the root `CMakeLists.txt`; older disabled targets may need maintenance before they compile against the current shared code.

## Upload firmware

### Initial upload over USB

Prepare the partitions as described above, then load the matching UF2:

```bash
sudo picotool load -x build/<project>/<target>.uf2
```

### Normal upload over Ethernet

Running applications accept EFU v1.2 updates over TCP port 4243. Upload the target's `.bin` file:

```bash
python3 efu_upload.py <board-ip> build/<project>/<target>.bin
```

Current commands are:

```bash
python3 efu_upload.py 192.168.14.228 build/kitchen_pwm/kitchen_pwm.bin
python3 efu_upload.py 192.168.178.225 build/stairs_ws2815/proj_stairs_ws2815.bin
python3 efu_upload.py 192.168.14.226 build/tree_ws2815/tree_leds.bin
```

Convenience scripts `upload_kitchen.sh`, `upload_stairs.sh`, and `upload_tree.sh` contain the same device-specific commands. EFU writes the inactive application slot, checks CRC32, and switches the boot selection. Device identity, network addresses, ports, pins, and firmware version are compile-time settings in each application's `config.h`; verify them before uploading to a different board.

> **Tree address note:** `upload_tree.sh` currently uses `192.168.14.226`, but the enabled address in `tree_ws2815/config.h` is `192.168.178.226`. Confirm the board's network before using the script.

## Diagnostics

- Telnet CLI: connect to the board on TCP port 5000 and use `help` or `status`.
- UDP debug receivers: `udp_dbg_client.py` and `service_dbg.py`.
- Foxglove helper scripts are available at the repository root and under `foxg/` for telemetry visualization.
