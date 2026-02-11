# Copilot Instructions for RP2350 WIZnet Embedded Projects

## Project Overview

This is a **Raspberry Pi Pico 2 (RP2350) multi-application firmware workspace** for embedded IoT devices using WIZnet W6100 Ethernet controller. The project manages multiple independent firmware builds (`kitchen_pwm`, `stairs_ws2815`, `tree_ws2815`, `scd30_meter`) that share common libraries but have device-specific configurations.

**Key Stack:**
- **MCU:** Raspberry Pi Pico 2 (RP2350), dual-core ARM Cortex-M33
- **Networking:** WIZnet W6100 (Ethernet + IP stack)
- **Security:** mbedTLS for SSL/TLS
- **Sensors:** VL53L8CX (Time-of-Flight), RD03D (Radar), SCD30 (CO2)
- **Output:** PWM control for RGBW lighting via DDP protocol
- **OTA:** Dual-partition firmware update via TCP/telnet

---

## Architecture & Data Flow

### Multi-Target Build Structure
- **Root CMakeLists.txt** defines RP2350 platform, includes SDK, and adds each project as subdirectory
- **Each app** (`kitchen_pwm/`, `stairs_ws2815/`, etc.) is independent binary with own CMakeLists.txt
- **Shared libraries** in `libraries/` (mbedTLS, ioLibrary_Driver, port layer)
- **Output:** Each app generates `.bin` in `build/<app>/` and is uploadable via OTA

### Hardware Layers (Layered Architecture)

```
Application Layer:
  ├─ main.c → event loop (timer callback + TCP/UDP services)
  ├─ pwm_api.c → DDP frame parsing + fade engine (zero-copy)
  ├─ rd03d_api.c → radar tracking + presence state machine
  └─ vl53l8cx_drv.c → ToF sensor initialization + data polling

Network Layer:
  ├─ network.c → UDP/TCP socket init, WIZnet IRQ handling
  ├─ telnet.c → CLI server for diagnostics + OTA commands
  └─ wizchip_custom.c → W6100 SPI setup + interrupt dispatch

Hardware Layer:
  ├─ libraries/port/ → WIZnet SPI driver, timer, board init
  ├─ hardware_spi, hardware_i2c, hardware_pwm (Pico SDK)
  └─ pico-sdk → bootloader, clock, interrupt, flash
```

**Data Flow Example (DDP Lighting):**
UDP packet → WIZnet IRQ → `process_ddp_udp()` → `pwm_rgbw_ddp_config()` → `pwm_rgbw_fade_to()` → PWM DMA on next poll

---

## Build System & Workflows

### Build Commands
```bash
# Full project build
ninja -C build

# Build single target
ninja -C build kitchen_pwm.elf

# Clean and rebuild
ninja -C build -t clean && ninja -C build
```

### Key CMake Configuration
- **W6100_EVB_PICO2** board target set in root CMakeLists.txt
- **Pico SDK 2.2.0** with deoptimized debug (`PICO_DEOPTIMIZED_DEBUG=1`)
- **Platform:** `rp2350` architecture
- **Disabled mbedTLS tests** to reduce build size

### Flash & Deploy

**Partition Management:**
- Defined in `w6100_partitions.json` (or `w6100_partitions_v2.json`)
- Convert to UF2: `picotool partition create w6100_partitions.json w6100_part.uf2`
- Load partition table: `picotool load w6100_part2.uf2`

**OTA Firmware Update (Primary Method):**
```bash
python3 efu_fw_upload_v12.py <board_ip> build/kitchen_pwm/kitchen_pwm.bin
```
- Kitchen: `192.168.14.228`
- Stairs: `192.168.178.225`
- Uploads via TCP to EFU (Embedded Firmware Upgrade) server running on board
- Validates CRC32, writes to alternate partition, sets boot flag

**Direct Flash (Debugging Only):**
- Connect via USB → UF2 loader or OpenOCD
- Use VS Code tasks: "Flash", "Rescue Reset" (see workspace tasks)

---

## Configuration & Device-Specific Setup

### Per-Device Configuration Files
Each application has a config header controlling:
- **Network:** MAC, IP, subnet, gateway, DNS (static or DHCP)
- **Flash Partitions:** Offsets for config/data sectors
- **TCP Ports:** CLI telnet port (5000), EFU port
- **Sensor Tuning:** PWM pins, I2C addresses, DDP channel offsets

**Example:** `kitchen_pwm/config_kitchen.h`
```c
#define NETINFO_IP      {192, 168, 14, 228}
#define NETINFO_MAC     {0x00, 0x08, 0xDC, 0x12, 0x34, 0x59}
#define CONFIG_FLASH_OFFSET 0x001f6000    // Last 40KB of flash
#define TCP_CLI_PORT    5000              // Telnet diagnostics
```

### Network Initialization Flow
1. `main()` → `stdio_init_all()` (UART/USB logging)
2. `wizchip_spi_initialize()` → W6100 SPI 40 MHz
3. `init_net_info()` → Load MAC/IP from config, apply static networking
4. `udp_socket_init()` + `udp_interrupts_enable()` → Bind UDP for DDP
5. `tcp_cli_service()` runs in main loop, polls telnet commands

---

## Key Design Patterns & Conventions

### Layered Driver Pattern (Two-Tier API)
- **`_drv.c`** = Low-level hardware binding (registers, SPI, I2C, interrupts)
- **`_api.c`** = High-level business logic (state machines, filtering, clients call this)

**Example:** VL53L8CX
- `vl53l8cx_drv.c` → I2C register reads, sensor state
- `vl53l8cx_api.c` → Presence detection, distance reporting

**Example:** PWM Lighting
- `pwm_drv.c` → PWM slice init, DMA setup, fade tick calculation
- `pwm_api.c` → Color structs, DDP frame parsing, fade engine

### Zero-Copy / DMA-First Patterns
- PWM fade engine uses `absolute_time_t` computed on poll, no ISR timer
- DDP frame writes directly to PWM slice without buffering
- UDP rx uses WIZnet's internal buffer, copied only once to app buffer

### Configuration via Compile-Time Constants
- No runtime config files initially; flash storage added via `flash_cfg.c`
- Device identity baked into config headers to avoid flashing wrong app
- Use `#define` macros for tuning (debounce, thresholds, fade durations)

### Telnet CLI for Diagnostics
- TCP server on port 5000 listens for commands
- Commands parse sensor data, firmware info, partition info
- Example: `telnet 192.168.14.228 5000` → type `help` or `status`
- Used for live testing without reflashing

---

## Integration Points & External Dependencies

### WIZnet W6100 Integration
- **SPI Interface:** 40 MHz (2 x 20 MHz read), controlled by `libraries/port/wizchip_spi.c`
- **IRQ Handling:** GPIO IRQ routed to `wizchip_custom.c` → socket callbacks
- **Includes:** ioLibrary_Driver (socket layer), firmware built from vendored copy

### Sensor Integration (I2C)
- **VL53L8CX (ToF):** I2C @ addr 0x29, provides 8×8 pixel depth map + object detection
- **RD03D (24GHz Radar):** I2C for config, UDP rx for target data (tracks position, speed, confidence)
- **SCD30 (CO2):** I2C sensor for meter variant

### mbedTLS Integration
- Included in vendor copy `libraries/mbedtls/`, built as static lib
- Used for SSL/TLS if secure OTA needed (currently commented out in most configs)
- Configured via `libraries/port/mbedtls/inc/ssl_config.h`

### Pico SDK Dependency
- **Location:** `~/.pico-sdk/sdk/2.2.0` (set by VS Code extension)
- **Key modules used:** hardware_spi, hardware_i2c, hardware_pwm, hardware_dma, boot_rom
- **Flash handling:** `hardware_flash` for OTA writes; manual erase/write via `flash_cfg.c`

---

## Common Tasks & Debugging

### Compile Errors Checklist
1. **Missing partition.json:** Ensure `w6100_partitions.json` is in root
2. **Undefined WIZnet macros:** Check `config_kitchen.h` has `_WIZCHIP_=W6100` defined
3. **Wrong Pico SDK path:** Verify `~/.pico-sdk/sdk/2.2.0` exists or update CMakeLists.txt `PICO_SDK_PATH`
4. **Missing libraries subdir:** Run from root; `libraries/` must contain ioLibrary_Driver, mbedtls, port copies

### Debugging Tips
- **Serial log:** Connect USB, watch `stdio_init_all()` output for boot messages
- **Telnet status:** `telnet <ip> 5000` → `status` command shows sensors, partition info
- **Partition info:** `picotool info -a` shows current boot partition, free space
- **Network debug:** Enable UDP packet logging in `network.c` `process_ddp_udp()`
- **PWM fade debug:** Add printf in `pwm_rgbw_poll()` to trace color transitions

### OTA Update Troubleshooting
- **CRC mismatch:** Binary corrupted during transfer; check network stability, retry
- **Partition full:** Reduce CONFIG_SIZE or DATA_SIZE in config header
- **Boot stuck:** Board looping between partitions; use "Rescue Reset" task or manual openocd

---

## File Organization by Role

| Role | Key Files |
|------|-----------|
| **App Entry** | `kitchen_pwm/main.c` |
| **Config** | `kitchen_pwm/config_kitchen.h`, `CMakeLists.txt` |
| **Networking** | `kitchen_pwm/network.c`, `kitchen_pwm/telnet.c` |
| **Firmware Update** | `kitchen_pwm/partition.c`, `kitchen_pwm/efu_update.c` |
| **Sensors** | `kitchen_pwm/vl53l8cx_drv.c`, `kitchen_pwm/rd03d_api.c` |
| **Lighting** | `kitchen_pwm/pwm_api.c`, `kitchen_pwm/pwm_drv.c` |
| **Vendor Libs** | `libraries/ioLibrary_Driver/`, `libraries/mbedtls/`, `libraries/port/` |
| **Deploy** | `efu_fw_upload_v12.py`, `run_kitchen.sh` |

---

## Critical Facts for Modifications

1. **Dual-partition boot:** Device alternates between two flash regions; OTA writes to inactive partition, then toggles flag
2. **W6100 IRQ synchronous:** Socket callbacks happen in ISR context; keep handlers minimal
3. **Main loop is cooperative:** No preemptive scheduler; timers use `repeating_timer_t` with callback
4. **GPIO allocation is fixed:** PWM slices, I2C pins hardcoded in driver init; check `libraries/port/board_pins.h` before reusing
5. **Flash sector aligned:** All config writes must be 4KB-aligned; partition offsets in `config_kitchen.h` are critical
