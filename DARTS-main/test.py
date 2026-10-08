import socket
import struct
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print("Connecting...")
s.connect(("127.0.0.1", 8888))
print("Connected!")

# Packet is 2116 bytes -> 4 (type MSG_LOGIN=0) + 32 (source="testuser") + 32 (target) + 2048 (data)
packet = struct.pack("<I 32s 32s 2048s", 0, b"testuser", b"", b"")
print(f"Sending packet of size {len(packet)}")
s.sendall(packet)

print("Waiting for response...")
try:
    s.settimeout(2.0)
    data = s.recv(2116)
    print(f"Received {len(data)} bytes")
    if len(data) > 0:
        typ, src, tgt, payload = struct.unpack("<I 32s 32s 2048s", data)
        print(f"Type: {typ}")
        print(f"Source: {src.strip(b'\x00').decode()}")
        print(f"Payload: {payload.strip(b'\x00').decode()}")
except Exception as e:
    print(f"Error: {e}")

s.close()
