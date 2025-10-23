import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(b"Hello1", ("192.168.178.225", 4048))
print("Sent test packet")

