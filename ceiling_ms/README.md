# MS72SF1 ceiling radar

This application uses the MinewSemi MS72SF1 datasheet v1.0.0 (sections 4–8).
The copied lighting/ToF and Ethernet services remain in the project; the ToF
sensor is disabled by default. The radar implementation has not yet been
verified against a physical module.

## Wiring and power

| MS72SF1 | RP2350 / supply |
| --- | --- |
| TX | GP15 / UART0 RX (`GPIO_FUNC_UART_AUX`) |
| RX | GP14 / UART0 TX (`GPIO_FUNC_UART_AUX`) |
| GND | Common ground |
| 3.3V | Regulated 3.3 V supply |

Verify that these pins are exposed and free on the actual board. The datasheet
specifies a 3.0–3.3 V operating supply and recommends at least 1 A supply
capacity. Do not assume the board's 3.3 V output can supply the radar; verify
it or use a suitable external supply with common ground. UART is 115200, 8N1,
no flow control. USB stdio is enabled and UART stdio is disabled.

## Start and inspect

The firmware does **not** automatically alter the radar's configuration or
start it. Connect to the telnet CLI (configured IP, port 5000) and send each
command separately, waiting for a response before the next:

```text
radar binary       # AT+DEBUG=3: select binary operating mode
radar status       # check the latest response, typically AT+OK
radar start        # AT+START
radar status       # inspect frame number, people and coordinates
radar stop         # AT+STOP
radar read         # AT+READ
radar at AT+TIME=100
```

`radar at` accepts one printable `AT+...` command and appends LF; it does not
validate sensor-specific parameters. Configuration commands may be persistent;
consult the datasheet before changing them. The status shows only the **last
newline-terminated text line**, not a complete multi-line AT response. Sending
a command does not confirm that the sensor accepted it. The shared TCP CLI
currently treats each TCP receive as a command rather than assembling a line;
use a client that sends each complete command in one write, e.g. `nc`, and wait
between commands.

## Data handling and limitations

The parser expects the documented 8-byte `01 02 03 04 05 06 07 08` header,
little-endian whole-frame length and frame number, point TLV 1 with zero
length, and track TLV 2 with 32 bytes per person. It accepts up to ten people;
coordinates are in metres and velocity in metres/second. It does not assume
a checksum, since the datasheet does not specify one. It rejects other frame
layouts and reports invalid frames; capture actual UART traffic if the module
uses a different variant or length convention.

UART RX interrupts feed a bounded ring; `radar status` reports dropped bytes.
Only the latest complete frame is retained. Presence is simply whether that
frame has any tracks: it is **not** a timeout, safety or occupancy decision.
The timestamp remains unchanged if the sensor stops reporting. Flash operations
or long interrupt-disabled periods can still cause hardware UART overruns.

Build with `cmake --build build --target ceiling_ms`. Host parser tests can be
run with:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iceiling_ms ceiling_ms/ms72sf1_parser.c ceiling_ms/tests/test_ms72sf1_parser.c -o /tmp/test_ms72sf1 && /tmp/test_ms72sf1
```
