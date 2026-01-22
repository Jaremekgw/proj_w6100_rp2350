## VL53L8CX (SPI) Wiring – RP2350 + Pololu-style Breakout
Document:
 - hardware: https://www.st.com/en/imaging-and-photonics-solutions/VL53L8CX.html
 - example: https://github.com/stm32duino/VL53L8CX
 

This project uses an **ST VL53L8CX Time-of-Flight sensor** connected via **SPI** to an
**RP2350-based WIZnet Pico2 board**.

This setup is bus-safe because:

Component	    SPI instance	    Pins	            CS
W6100	        SPI0	            board default	    dedicated
VL53L8CX	    SPI1	            GP7–GP10	        GP10


The sensor module is a **Pololu-style breakout** (also sold on AliExpress) equipped with:
- Onboard **1.8 V regulators**
- **WTXS0104E bidirectional level shifter**
- Required pull-ups / pull-downs for SPI operation

This allows **direct 3.3 V GPIO connection** from RP2350 without external level shifting.

---

### Electrical assumptions

- `VIN` accepts **3.3 V**
- Logic level shifting (3.3 V ↔ 1.8 V) is handled onboard
- SPI mode is selected via `SPI_I2C_N`
- `SPI_I2C_N` has an onboard **10 kΩ pull-down**, therefore **must be driven HIGH**
- `INT` is open-drain, active-low
- SPI mode: **Mode 0 (CPOL=0, CPHA=0)**

---

### Pin mapping

| RP2350 GPIO | VL53L8CX board pin | Signal description |
|------------|-------------------|--------------------|
| 3V3       | VIN               | Sensor power input    |
| GND       | GND               | Ground                |
| GP8       | SCL / CLK         | SPI clock             |
| GP9       | SDA / MOSI        | SPI MOSI              |
| GP7       | MISO              | SPI MISO              |
| GP10      | NCS               | SPI chip select (active LOW)  |
| GP6       | INT               | Interrupt output (active LOW) |
| GP11      | LPn               | Low-power / interface enable  | 
| 3V3       | SPI_I2C_N         | Interface select (SPI = HIGH) |

---

### Special control pins

#### `SPI_I2C_N`
- **Purpose:** Selects SPI vs I²C
- **Board behavior:** Pulled down by 10 kΩ
- **Required action:** Drive **HIGH**
- **Implementation:** Connected directly to **3V3**

```text
SPI_I2C_N = 1 → SPI enabled
SPI_I2C_N = 0 → I²C enabled
