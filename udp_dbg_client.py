# UDP debug client for the RD03D radar sensor. Connects to a device over UDP,
# sends periodic HELLO keepalive messages, receives binary radar frames and decodes
# them into human-readable target data (position, velocity, distance).
# Supports clean exit via ESC or Ctrl+C.
#
# RD03D Frame Format, Expected packet size: 32 bytes
# Field		Type		Size		Description
# rx_time_ms	uint32		4 B		Timestamp in milliseconds
# target[0..2]	struct × 3	9 B each	X, Y, velocity, distance, active flag
# ready		uint8		1 B		Frame ready flag
#
# Usage:
#   python3 udp_dbg_client.py <device_ip>
#
# Example:
#   python3 udp_dbg_client.py 192.168.14.228
#

import socket
import time
import select
import struct
import argparse
import sys
import termios
import tty

DEVICE_PORT = 3000	# Target device UDP port
HELLO_INTERVAL = 2.0	# Keepalive interval in seconds
BUFFER_SIZE = 2048	# Max received packet size in bytes

RD03D_OBJECT_SLOTS = 3	# Number of target slots per frame


def parse_target(data, offset):
    x_raw, y_raw, v_raw, dist_mm, active = struct.unpack_from("<hhhHB", data, offset)
    return {
        "x_raw": x_raw,
        "y_raw": y_raw,
        "v_raw": v_raw,
        "dist_mm": dist_mm,
        "active": bool(active)
    }


def parse_rd03d_frame(data):
    rx_time_ms = struct.unpack_from("<I", data, 0)[0]

    offset = 4
    targets = []

    for _ in range(RD03D_OBJECT_SLOTS):
        targets.append(parse_target(data, offset))
        offset += 9

    ready = struct.unpack_from("<B", data, offset)[0]

    return {
        "rx_time_ms": rx_time_ms,
        "targets": targets,
        "ready": bool(ready)
    }


def print_frame(frame):
    print(f"\nFrame time: {frame['rx_time_ms']} ms  ready={frame['ready']}")

    for i, t in enumerate(frame["targets"]):
        if t["active"]:
            print(
                f"  target[{i}] "
                f"x={t['x_raw']}mm "
                f"y={t['y_raw']}mm "
                f"v={t['v_raw']} "
                f"d={t['dist_mm']}mm"
            )
        else:
            print(f"  target[{i}] inactive")


# -------- keyboard handling (ESC detection) --------

def enable_raw_mode():
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    tty.setcbreak(fd)
    return old_settings


def restore_terminal(old_settings):
    termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, old_settings)


def esc_pressed():
    if select.select([sys.stdin], [], [], 0)[0]:
        ch = sys.stdin.read(1)
        return ch == '\x1b'  # ESC
    return False


# -------- main --------

def main():
    parser = argparse.ArgumentParser(description="RD03D UDP debug client")
    parser.add_argument("ip", help="device IP address")
    args = parser.parse_args()

    device_ip = args.ip

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)

    last_hello = 0

    print(f"Listening for RD03D data from {device_ip}:{DEVICE_PORT}")
    print("Press ESC or Ctrl-C to exit")

    old_term = enable_raw_mode()

    try:
        while True:

            # ESC handling
            if esc_pressed():
                print("\nESC pressed, exiting...")
                break

            now = time.time()

            if now - last_hello > HELLO_INTERVAL:
                sock.sendto(b"HELLO", (device_ip, DEVICE_PORT))
                last_hello = now
                print(f"[TX] HELLO -> {device_ip}:{DEVICE_PORT}")

            readable, _, _ = select.select([sock], [], [], 0.1)

            if sock in readable:
                data, addr = sock.recvfrom(BUFFER_SIZE)

                print(f"[RX] {len(data)} bytes from {addr}")

                if len(data) == 32:
                    frame = parse_rd03d_frame(data)
                    print_frame(frame)
                else:
                    print("unexpected packet length")
                    print(data.hex(" "))

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\nCtrl-C received, exiting...")

    finally:
        restore_terminal(old_term)
        sock.close()
        print("Socket closed")


if __name__ == "__main__":
    main()

