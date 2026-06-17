#
# Publishes plain logs, it works on log panel.
# 
# Start:
#  $ python3 foxglove_bridge_logs.py 192.168.14.228
#
# On foxglove  connection WebSocket:   ws://localhost:8765

import argparse
import socket
import struct
import time

import foxglove
from foxglove.schemas import Log, LogLevel, Timestamp

DEVICE_PORT = 3000
BUFFER_SIZE = 2048
HELLO_INTERVAL = 2.0
RD03D_OBJECT_SLOTS = 3
EXPECTED_PACKET_SIZE = 32


def parse_target(data: bytes, offset: int) -> dict:
    x_raw, y_raw, v_raw, dist_mm, active = struct.unpack_from("<hhhHB", data, offset)
    return {
        "x_raw": x_raw,
        "y_raw": y_raw,
        "v_raw": v_raw,
        "dist_mm": dist_mm,
        "active": bool(active),
    }


def parse_rd03d_frame(data: bytes) -> dict:
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
        "ready": bool(ready),
    }


def format_frame_for_log(frame: dict) -> str:
    lines = [f"rx_time_ms={frame['rx_time_ms']} ready={frame['ready']}"]

    for i, t in enumerate(frame["targets"]):
        if t["active"]:
            lines.append(
                f"target[{i}] active=1 "
                f"x={t['x_raw']}mm "
                f"y={t['y_raw']}mm "
                f"v={t['v_raw']} "
                f"d={t['dist_mm']}mm"
            )
        else:
            lines.append(f"target[{i}] active=0")

    return " | ".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="RD03D Foxglove bridge over UDP")
    parser.add_argument("ip", help="device IPv4 address")
    parser.add_argument("--device-port", type=int, default=DEVICE_PORT, help="device UDP port")
    parser.add_argument("--hello-interval", type=float, default=HELLO_INTERVAL, help="HELLO interval in seconds")
    args = parser.parse_args()

    device_ip = args.ip
    device_port = args.device_port
    hello_interval = args.hello_interval

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)

    foxglove.start_server()

    print(f"Foxglove server started")
    print(f"Sending HELLO to {device_ip}:{device_port}")
    print("Open a Log panel in Foxglove and subscribe to /rd03d")
    print("Press Ctrl-C to exit")

    last_hello = 0.0

    try:
        while True:
            now = time.time()

            if now - last_hello >= hello_interval:
                sock.sendto(b"HELLO", (device_ip, device_port))
                last_hello = now
                print(f"[TX] HELLO -> {device_ip}:{device_port}")

            readable, _, _ = select.select([sock], [], [], 0.1)

            if sock in readable:
                data, addr = sock.recvfrom(BUFFER_SIZE)
                print(f"[RX] {len(data)} bytes from {addr}")

                if len(data) != EXPECTED_PACKET_SIZE:
                    msg = f"unexpected packet length={len(data)} hex={data.hex(' ')}"
                    print(msg)
                    foxglove.log(
                        "/rd03d",
                        Log(
                            timestamp=Timestamp.now(),
                            level=LogLevel.Warning,
                            message=msg,
                        ),
                    )
                    continue

                frame = parse_rd03d_frame(data)
                message = format_frame_for_log(frame)

                print(message)

                foxglove.log(
                    "/rd03d",
                    Log(
                        timestamp=Timestamp.now(),
                        level=LogLevel.Info,
                        message=message,
                    ),
                )

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\nCtrl-C received, exiting...")

    finally:
        sock.close()
        print("Socket closed")


if __name__ == "__main__":
    import select

    main()

