# Send DDP packets for project stairs_ws2815
# leds: 16 * 57 = 912
# test UDP only:
#	$ echo -n "Hello" | nc -u 192.168.178.225 4048
#	$ echo -n "Hello" | nc -u -w 1 192.168.178.225 4048
#
# test DDP:
#	$ python3 send_ddp_test.py

import socket
import struct
import time

# Pico IP address and DDP port
TARGET_IP = "192.168.178.225"   # WIZnet board
TARGET_PORT = 4048              # DDP UDP port

# NUM_LEDS = 912
NUM_LEDS = 880
# NUM_LEDS = 400
BYTES_PER_LED = 3
CHUNK_SIZE  = 1242              # ensures each UDP datagram < ~1450 bytes, max chank size is 1242

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def send_ddp_frame(rgb_bytes):
    """Send large frame as DDP fragments (≤ CHUNK_SIZE each)."""
    total = len(rgb_bytes)
    offset = 0
    frag = 0
    while offset < total:
        chunk = rgb_bytes[offset:offset+CHUNK_SIZE]
        flags1 = 0x40  # non-zero marker
        if (offset + CHUNK_SIZE) >= total:
            flags1 |= 0x01  # PUSH on last fragment
            
        # flags1 = 0x41 if (offset + CHUNK_SIZE) >= total else 0x40  # PUSH only on last packet
        flags2 = 0x00
        dtype  = 0x0000
        length = len(chunk)
        # big-endian header: flags1, flags2, dtype, offset, length
        # header = struct.pack(">BBHIH", flags1, flags2, dtype, offset, length)
        header = struct.pack(">BBHIH", flags1, 0, 0, offset, length)
        packet = header + chunk
        hdr_hex = " ".join(f"{b:02X}" for b in header)
        print(f"  DDP header: {hdr_hex}")
        
        # sent = sock.sendto(bytes(packet), (TARGET_IP, TARGET_PORT))
        sent = sock.sendto(packet, (TARGET_IP, TARGET_PORT))
        print(f"  Sent DDP fragment #{frag} offset={offset}, length={length}, total packet={len(packet)} bytes (sent={sent})")
        frag += 1
        offset += length
        # small delay between fragments helps on slower networks
        time.sleep(0.002)   # minimum 12ms delay between fragments
        

# demo animation
#for step in range(0, 256, 10):
for step in range(0, 16, 10):
    r, g, b = step, 0, 0
    payload = bytes([r, g, b] * NUM_LEDS)
    print(f"\nSending frame red={r}, total={len(payload)} bytes, CHUNK_SIZE={CHUNK_SIZE}")
    send_ddp_frame(payload)
    time.sleep(0.1)

## Make a DDP header:
## flags1, flags2, type, offset(4B), data_len(2B)
#def make_ddp_header(payload, push=True):
#    flags1 = 0x01 if push else 0x00
#    flags2 = 0x00
#    dtype = 0x0000
#    offset = 0
#    length = len(payload)
#    header = struct.pack(">BBHIH", flags1, flags2, dtype, offset, length)
#    return header


## Example: fill LEDs with a moving red intensity
## for step in range(0, 256, 10):
#for step in range(0, 26, 10):
#    r = step
#    g = 0
#    b = 0
#    payload = bytes([r, g, b] * NUM_LEDS)
#    header = make_ddp_header(payload)
#    packet = header + payload
#    print(f"Payload={len(payload)}, Header={len(header)}, Total={len(packet)}")
#
#    sock.sendto(packet, (TARGET_IP, TARGET_PORT))
#    print(f"Sent DDP frame with red={r}")
#
#    time.sleep(0.1)


