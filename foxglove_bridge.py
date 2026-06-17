#
# Send structured data
#
# Usage:
#  $ python3 foxglove_bridge.py 192.168.14.228
#

import argparse
import socket
import struct
import time
import select
import json

import foxglove

DEVICE_PORT = 3000
BUFFER_SIZE = 2048
HELLO_INTERVAL = 2.0
RD03D_OBJECT_SLOTS = 3
EXPECTED_PACKET_SIZE = 32


def parse_target(data, offset):
    x, y, v, d, active = struct.unpack_from("<hhhHB", data, offset)
    return x, y, v, d, bool(active)


def parse_frame(data):
    rx_time_ms = struct.unpack_from("<I", data, 0)[0]

    offset = 4
    targets = []

    for _ in range(RD03D_OBJECT_SLOTS):
        targets.append(parse_target(data, offset))
        offset += 9

    ready = struct.unpack_from("<B", data, offset)[0]

    return rx_time_ms, targets, bool(ready)


def build_plot_message(rx_time_ms, targets, ready):
    msg = {
        "timestamp": time.time(),
        "ready": ready,
        "rx_time_ms": rx_time_ms,
    }

    for i, (x, y, v, d, active) in enumerate(targets):
        if active:
            msg[f"t{i}_x"] = x
            msg[f"t{i}_y"] = y
            msg[f"t{i}_v"] = v
            msg[f"t{i}_d"] = d
        else:
            msg[f"t{i}_x"] = 0
            msg[f"t{i}_y"] = 0
            msg[f"t{i}_v"] = 0
            msg[f"t{i}_d"] = 0

    return msg


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("ip", help="device IP")
    args = parser.parse_args()

    device_ip = args.ip

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)

    foxglove.start_server()

    print("Foxglove bridge started (2D plot ready)")
    print("Use Plot panel with:")
    print("  X axis: t0_x")
    print("  Y axis: t0_y")
    print("Press Ctrl-C to exit")

    last_hello = 0

    try:
        while True:
            now = time.time()

            if now - last_hello > HELLO_INTERVAL:
                sock.sendto(b"HELLO", (device_ip, DEVICE_PORT))
                last_hello = now

            readable, _, _ = select.select([sock], [], [], 0.1)

            if sock in readable:
                data, _ = sock.recvfrom(BUFFER_SIZE)

                if len(data) != EXPECTED_PACKET_SIZE:
                    continue

                rx_time_ms, targets, ready = parse_frame(data)

                msg = build_plot_message(rx_time_ms, targets, ready)

                # still using log transport, but structured
                foxglove.log(
                    "/rd03d_plot",
                    foxglove.schemas.Log(
                        timestamp=foxglove.schemas.Timestamp.now(),
                        level=foxglove.schemas.LogLevel.Info,
                        message=json.dumps(msg),
                    ),
                )

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\nExiting...")

    finally:
        sock.close()


if __name__ == "__main__":
    main()

