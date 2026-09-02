# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A multi-application firmware **workspace** for the Raspberry Pi Pico 2 (RP2350, dual-core Cortex-M33) with a WIZnet **W6100** Ethernet controller (board `W6100_EVB_PICO2`). One CMake tree builds several independent firmware binaries that share `common/` code and vendored `libraries/`. Each app targets a specific physical device (kitchen, stairs, tree, etc.) and is deployed over the network via OTA.

Apps (each is a subdirectory + a CMake target added in the root `CMakeLists.txt`):
- `kitchen_pwm/` — RGBW PWM lighting driven by the DDP protocol; VL53L8CX ToF + RD03D 24 GHz radar presence sensing. The actively developed app.
- `stairs_ws2815/`, `tree_ws2815/` — WS2815 addressable LED strips via PIO/DMA.
- `scd30_meter/` — SCD30 CO₂ sensor (currently commented out of the root build).
- `layout_seeder/` — utility target for seeding the flash partition layout.

Note: `add_subdirectory(...)` lines in the root `CMakeLists.txt` control which apps build. The `all_projects` meta-target currently depends only on `kitchen_pwm`.

## Build

The Pico VS Code extension supplies the SDK at `~/.pico-sdk/sdk/2.2.0` (pinned in `CMakePresets.json` and `wiznet_pico_c_sdk_version.cmake`).

```bash
# Configure (Ninja, Debug) — writes to ./build
cmake --preset pico

# Build everything
cmake --build build

# Build one app
cmake --build build --target kitchen_pwm

# Clean rebuild
cmake --build build --target clean && cmake --build build
```

Build artifacts land in `build/<app>/<target>.{elf,bin,uf2,map}`. `pico_add_extra_outputs` produces the `.uf2`/`.bin`; OTA uploads the `.bin`.

There is **no test suite or linter** in this workspace. Strict warnings (`-Wall -Wextra -Wconversion -Wsign-conversion -Wshadow`) are applied per-target via the `enable_strict_warnings()` function defined in the root `CMakeLists.txt` — call it for any new target. SDK/vendor warnings are selectively suppressed.

## Deploy (OTA is the normal path — no USB reflash needed)

Firmware is updated over TCP using the **EFU** (Ethernet Firmware Update v1.2) protocol on port **4243**, implemented in `common/efu/efu_update.c`. The host side is `efu_upload.py`:

```bash
python3 efu_upload.py <board_ip> build/kitchen_pwm/kitchen_pwm.bin
```

Convenience wrappers `upload_kitchen.sh` / `upload_stairs.sh` / `upload_tree.sh` hardcode the IP + bin path per device (kitchen `192.168.14.228`, stairs `192.168.178.225`, tree `192.168.14.226`). The device validates a CRC32, writes to the inactive flash partition, then flips the boot flag.

Partition tables are defined in each app's `w6100_partitions.json` (embedded into the binary via `pico_embed_pt_in_binary`). To (re)write the on-device table manually:

```bash
sudo picotool partition create w6100_partitions.json w6100_part.uf2
sudo picotool load w6100_part.uf2
sudo picotool info -a        # show active partition / free space
```

## Architecture

### Two-tier driver pattern (key convention)
Hardware peripherals are split into two files:
- **`*_drv.c`** — low-level hardware binding: registers, SPI/I2C transactions, DMA/PIO setup, ISR plumbing.
- **`*_api.c`** — high-level logic: state machines, filtering, color/fade engines. Application code calls only the `_api` layer.

Examples: `vl53l8cx_drv.c`/`vl53l8cx_api.c` (ToF), `pwm_drv.c`/`pwm_api.c` (lighting), `rd03d_drv.c`/`rd03d_api.c` (radar). A `*_cli.c` (e.g. `rd03d_cli.c`) registers the peripheral's commands with the telnet CLI.

### Layering
- **Application** — per-app `main.c` runs a cooperative loop: a `repeating_timer_t` callback drives periodic work, and TCP/UDP services are polled. No preemptive scheduler.
- **Network** — shared in `common/network/`: `network.c` (socket init + WIZnet IRQ), `tcp_cli.c` (telnet diagnostic CLI), `dbg_api.c` (UDP debug streaming). Per-app `telnet.c` wires app-specific commands.
- **WIZnet glue** — `common/wiznet/wizchip_custom.c` sets up W6100 SPI (40 MHz) and dispatches the GPIO IRQ to socket callbacks. Socket callbacks run in **ISR context — keep them minimal.**
- **Vendor** — `libraries/{ioLibrary_Driver,mbedtls,port}/` are vendored copies from WIZnet-PICO-C (see `tmp_app.md` for provenance/refresh steps). `libraries/port/` holds the WIZnet SPI driver, timer, and board init.

### Shared code in `common/` (built as the `common` library, linked by every app)
- `board/partition.c` — partition table inspection / boot-flag logic.
- `efu/efu_update.c` — the EFU OTA server (the device side of `efu_upload.py`).
- `flash/flash_cfg.c` — 4 KB-aligned flash config storage.
- `network/`, `wiznet/`, `utils/`, `diagnostic/` as above.

### Per-device config: `<app>/config.h`
Each app's `config.h` is the single source of device identity and tuning — `PROJECT_NAME`, `FW_VERSION`, static `NETINFO_*` (IP/MAC/subnet/gateway), `TCP_CLI_PORT` (5000), socket numbers, flash offsets, sensor/PWM pin assignments. Identity is compile-time baked so you can't flash the wrong app to a device. Editing config means rebuilding + re-uploading.

### Diagnostics
- **Telnet CLI** on port 5000: `telnet <board_ip> 5000` then `help`/`status`. Used for live inspection without reflashing.
- **UDP debug**: `udp_dbg_client.py` / `service_dbg.py` receive streamed debug output (`common/network/dbg_api.c`).
- **Foxglove**: `foxg/` is a Python WebSocket bridge for visualizing telemetry in the Foxglove app (`ws://localhost:8765`). Run inside the `.venv`: `source .venv/bin/activate && python foxg/quickstart/main.py`. See `tmp_app.md` §6.

## Reference docs in-repo
- `tmp_app.md` — the original setup runbook (library vendoring, partitions, picotool, Foxglove). Authoritative for environment setup.
- `.github/copilot-instructions.md` — broader prose on data flow and debugging. Useful but partly stale: it references `config_kitchen.h` and `efu_fw_upload_v12.py`, which are actually `config.h` and `efu_upload.py` in the current tree.
- Per-app `README.md` where present.
