#
# Publishes plain logs, it works on log panel.
# 
# Start:
#  $ python3 foxglove_bridge_logs.py 192.168.14.228
#
# On foxglove  connection WebSocket:   ws://localhost:8765

import argparse
import select
import socket
import struct
import time

import foxglove
from foxglove.channels import (
    FrameTransformChannel,
    LogChannel,
    SceneUpdateChannel,
)
from foxglove.schemas import (
    Color,
    FrameTransform,
    Log,
    LogLevel,
    Pose,
    Quaternion,
    SceneEntity,
    SceneUpdate,
    SpherePrimitive,
    Timestamp,
    Vector3,
)

DEVICE_PORT = 3000
BUFFER_SIZE = 2048
HELLO_INTERVAL = 2.0
RD03D_OBJECT_SLOTS = 3
EXPECTED_PACKET_SIZE = 32
MCAP_FILE = "rd03d_logs.mcap"

# 3D scene settings
WORLD_FRAME_ID = "world"          # root frame; use this as the 3D panel "Display frame"
SCENE_FRAME_ID = "rd03d"          # frame the radar targets live in
TARGET_SPHERE_M = 0.15            # sphere diameter in metres
MM_TO_M = 1.0 / 1000.0           # RD03D reports millimetres; 3D panel uses metres
TF_INTERVAL = 0.5                 # how often to (re)publish the static transform, seconds


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


def build_world_transform() -> FrameTransform:
    """Static transform locating the radar frame in the world frame.

    Published periodically so the 3D panel always has a coordinate frame to
    anchor to, regardless of its "Display frame" setting. Identity placement
    for now; adjust translation/rotation to position the sensor in the room.
    """
    return FrameTransform(
        timestamp=Timestamp.now(),
        parent_frame_id=WORLD_FRAME_ID,
        child_frame_id=SCENE_FRAME_ID,
        translation=Vector3(x=0.0, y=0.0, z=0.0),
        rotation=Quaternion(x=0.0, y=0.0, z=0.0, w=1.0),
    )


def build_scene_update(frame: dict) -> SceneUpdate:
    """One sphere per active target, placed at its (x, y) position in metres.

    A fresh SceneEntity per slot id replaces the previous one each frame, so
    inactive/disappeared targets are cleared automatically.
    """
    entities = []
    for i, t in enumerate(frame["targets"]):
        spheres = []
        if t["active"]:
            spheres.append(
                SpherePrimitive(
                    pose=Pose(
                        position=Vector3(
                            x=t["x_raw"] * MM_TO_M,
                            y=t["y_raw"] * MM_TO_M,
                            z=0.0,
                        ),
                        orientation=Quaternion(x=0.0, y=0.0, z=0.0, w=1.0),
                    ),
                    size=Vector3(x=TARGET_SPHERE_M, y=TARGET_SPHERE_M, z=TARGET_SPHERE_M),
                    color=Color(r=1.0, g=0.2, b=0.0, a=1.0),
                )
            )
        # Emit the entity every frame (with or without a sphere) so a target
        # that goes inactive is removed from the scene.
        entities.append(
            SceneEntity(id=f"target_{i}", frame_id=SCENE_FRAME_ID, spheres=spheres)
        )

    return SceneUpdate(entities=entities)


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

    # Create a channel up front (like the quickstart example creates
    # SceneUpdateChannel / Channel) and log every message onto it.
    log_channel = LogChannel("/rd03d")
    scene_channel = SceneUpdateChannel("/rd03d/scene")
    tf_channel = FrameTransformChannel("/tf")

    # Record to an MCAP file as well as streaming to the live app.
    with foxglove.open_mcap(MCAP_FILE):
        foxglove.start_server(host="0.0.0.0", port=8765)

        print(f"Foxglove server started")
        print(f"Recording to {MCAP_FILE}")
        print(f"Sending HELLO to {device_ip}:{device_port}")
        print("Log panel:  subscribe to /rd03d")
        print(f"3D panel:   subscribe to /rd03d/scene (set Display frame = '{WORLD_FRAME_ID}')")
        print("Press Ctrl-C to exit")

        last_hello = 0.0
        last_tf = 0.0

        try:
            while True:
                now = time.time()

                if now - last_hello >= hello_interval:
                    sock.sendto(b"HELLO", (device_ip, device_port))
                    last_hello = now
                    print(f"[TX] HELLO -> {device_ip}:{device_port}")

                # Keep the world -> rd03d transform alive so the 3D panel always
                # has a frame to anchor to, even before any target arrives.
                if now - last_tf >= TF_INTERVAL:
                    tf_channel.log(build_world_transform())
                    last_tf = now

                readable, _, _ = select.select([sock], [], [], 0.1)

                if sock in readable:
                    data, addr = sock.recvfrom(BUFFER_SIZE)
                    print(f"[RX] {len(data)} bytes from {addr}")

                    if len(data) != EXPECTED_PACKET_SIZE:
                        msg = f"unexpected packet length={len(data)} hex={data.hex(' ')}"
                        print(msg)
                        log_channel.log(
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

                    log_channel.log(
                        Log(
                            timestamp=Timestamp.now(),
                            level=LogLevel.Info,
                            message=message,
                        ),
                    )

                    # 3D visualisation: draw each active target as a sphere
                    scene_channel.log(build_scene_update(frame))

                time.sleep(0.01)

        except KeyboardInterrupt:
            print("\nCtrl-C received, exiting...")

        finally:
            sock.close()
            print("Socket closed")


if __name__ == "__main__":
    main()

