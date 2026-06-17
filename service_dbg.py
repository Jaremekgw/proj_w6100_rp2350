#/bin/python3

# UDP debug client for communicating with an embedded device (or any UDP endpoint).
# Periodically sends a HELLO keepalive message to the target device and prints any
# received response as a hex dump to the console.
#
# Constant		Default		Description
# DEVICE_IP		192.168.14.228	Target device IP address
# DEVICE_PORT		3000		Target device UDP port
# HELLO_INTERVAL	2.0		Keepalive send interval in seconds
# BUFFER_SIZE		2048		Max received packet size in bytes
#
# Usage:
#   $ python3 service_dbg.py
#
# Example output
#   UDP debug client started
#   [TX] HELLO -> 192.168.14.228:3000
#   [RX] 6 bytes from ('192.168.14.228', 3000)
#   41 42 43 44 45 46
#   [TX] HELLO -> 192.168.14.228:3000


import socket
import time
import select

DEVICE_IP = "192.168.14.228"   # change to your device IP
DEVICE_PORT = 3000           # device UDP port

HELLO_INTERVAL = 2.0         # seconds
BUFFER_SIZE = 2048


def main():

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)

    last_hello = 0

    print("UDP debug client started")

    while True:

        now = time.time()

        # send HELLO periodically
        if now - last_hello > HELLO_INTERVAL:
            msg = b"HELLO"
            sock.sendto(msg, (DEVICE_IP, DEVICE_PORT))
            last_hello = now
            print(f"[TX] HELLO -> {DEVICE_IP}:{DEVICE_PORT}")

        # wait for incoming packets
        readable, _, _ = select.select([sock], [], [], 0.1)

        if sock in readable:
            data, addr = sock.recvfrom(BUFFER_SIZE)

            print(f"[RX] {len(data)} bytes from {addr}")

            # print hex dump
            print(data.hex(" "))

        time.sleep(0.01)


if __name__ == "__main__":
    main()

