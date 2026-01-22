#!/usr/bin/env python3
import socket, struct, sys, zlib
import time

# EFU (Ethernet Firmware Update) v1.2 upload protocol
# [3 bytes]  Stamp (0xD1 0x36 0x4A)
# [1 byte ]  Protocol version (0x12 → v1.2)
# [4 bytes]  Image size (big-endian)
# [...   ]   Image data (size bytes)
# [4 bytes]  CRC32 of image data (big-endian)

# Example usage:
# python efu_fw_upload_v12.py 192.168.178.225 build/stairs_ws2815/proj_stairs_ws2815.bin

PROTO = 0x12
STAMP = b"\xD1\x36\x4A"
# HEADER = 8
CHUNK = 2048  # must match device
TCP_EFU_SOCKET = 4243

ack_count = 0

if len(sys.argv) != 3:
    print("Usage: ota_send_v12.py <ip> <binfile>")
    sys.exit(1)

ip = sys.argv[1]
path = sys.argv[2]

with open(path, "rb") as f:
    fw = f.read()

size = len(fw)
crc = zlib.crc32(fw) & 0xFFFFFFFF

print(f"Firmware size: {size} bytes")
print(f"CRC32: {crc:08X}")

header = STAMP + bytes([PROTO]) + struct.pack(">I", size)
assert len(header) == 8

sock = socket.socket()
sock.connect((ip, TCP_EFU_SOCKET))
sock.settimeout(5.0)

# Send header
sock.sendall(header)

for count in range(4, 0, -1):
    print(f"Wait for erase: {count}", end="\r")
    time.sleep(1)

# Wait for ACK after header
ack = sock.recv(2)
if ack != b"HD":
    print("Error: no ACK after header, got:", ack)
    sys.exit(1)

# Send image in chunks with ACKs
offset = 0
while offset < size:
    chunk = fw[offset: offset+CHUNK]
    sock.sendall(chunk)


    offset += len(chunk)
    print(f"{offset}/{size} bytes sent", end="\r")
    # print(f"{offset}/{size} bytes sent\r")
    # print(f"Sent {len(chunk)}  {offset}/{size} bytes sent, rec ack {ack_count}")

# Wait for ACK
ack = sock.recv(2)
if ack != b"OK":
    print("\nError: no ACK after data transfered:", ack)
    sys.exit(1)
    
# print(f"\r\nSending CRC32")
# Send CRC32 trailer
sock.sendall(struct.pack(">I", crc))

ack = sock.recv(2)
ack_count = ack_count + 1
if ack != b"CC":
    print(f"Error: no ACK after CRC trailer:{ack}  rec ack: {ack_count}")
    sys.exit(1)

print("\nDone.")
sock.close()

