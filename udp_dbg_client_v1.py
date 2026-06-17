# Proto version for development, later to delete
#
# python3 udp_dbg_client.py 192.168.14.228
#

import socket
import time
import select
import struct
import argparse

DEVICE_PORT = 3000
HELLO_INTERVAL = 2.0
BUFFER_SIZE = 2048

RD03D_OBJECT_SLOTS = 3


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


def main():

    parser = argparse.ArgumentParser(description="RD03D UDP debug client")
    parser.add_argument("ip", help="device IP address")

    args = parser.parse_args()

    device_ip = args.ip

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)

    last_hello = 0

    print(f"Listening for RD03D data from {device_ip}:{DEVICE_PORT}")

    while True:

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


if __name__ == "__main__":
    main()

