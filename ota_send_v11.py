#!/usr/bin/env python3
import socket, struct, sys

PROTO_VER = 0x11
STAMP = b"\xD1\x36\x4A"
HEADER = 8
CHUNK = 2048  # must match device

if len(sys.argv) != 3:
    print("Usage: ota_send_v11.py <ip> <binfile>")
    sys.exit(1)

ip = sys.argv[1]
path = sys.argv[2]

with open(path, "rb") as f:
    fw = f.read()

size = len(fw)
print("Firmware size:", size)

sock = socket.socket()
sock.connect((ip, 4242))
sock.settimeout(5.0)

# Send header: stamp + version + size
header = STAMP + bytes([PROTO_VER]) + struct.pack(">I", size)
sock.sendall(header)

offset = 0
while offset < size:
    chunk = fw[offset: offset+CHUNK]
    sock.sendall(chunk)

    # Wait for ACK
    ack = sock.recv(2)
    if ack != b"OK":
        print("Error: no ACK, received:", ack)
        sys.exit(1)

    offset += len(chunk)
    print(f"{offset}/{size} bytes sent", end="\r")

print("\nDone.")
sock.close()

