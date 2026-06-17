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
from foxglove.schemas import Log, LogLevel, Timestamp

DEVICE_PORT = 3000
BUFFER_SIZE = 2048
HELLO_INTERVAL = 2.0
RD03D_OBJECT_SLOTS = 3
EXPECTED_PACKET_SIZE = 32


def parse_target(data, offset):
    x, y, v, d, active = struct.unpack_from("<hhhHB", data, offset)
    return {
        "x_mm": x,
        "y_mm": y,
        "v_raw": v,
        "dist_mm": d,
        "active": bool(active),
    }


def parse_frame(data):
    rx_time_ms = struct.unpack_from("<I", data, 0)[0]

    offset = 4
    targets = []
    for _ in range(RD03D_OBJECT_SLOTS):
        targets.append(parse_target(data, offset))
        offset += 9

    ready = struct.unpack_from("<B", data, offset)[0]

    return {
        "timestamp": time.time(),
        "rx_time_ms": rx_time_ms,
        "ready": bool(ready),
        "targets": targets,
    }


def main():
    parser = argparse.ArgumentParser(description="RD03D Foxglove bridge")
    parser.add_argument("ip", help="device IP")
    args = parser.parse_args()

    device_ip = args.ip

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)

    foxglove.start_server()

    print("Foxglove bridge started (JSON over Log)")
    print("Open Log panel → topic /rd03d")
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
                data, addr = sock.recvfrom(BUFFER_SIZE)

                if len(data) != EXPECTED_PACKET_SIZE:
                    continue

                frame = parse_frame(data)

                # ✅ structured JSON inside log
                foxglove.log(
                    "/rd03d",
                    Log(
                        timestamp=Timestamp.now(),
                        level=LogLevel.Info,
                        message=json.dumps(frame),
                    ),
                )

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\nCtrl-C received, exiting...")

    finally:
        sock.close()
        print("Socket closed")


if __name__ == "__main__":
    main()

