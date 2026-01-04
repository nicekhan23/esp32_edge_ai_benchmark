import serial
import time
import socket

gen = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Replace with your Windows laptop IP
WINDOWS_IP = "192.168.1.120"  # Find with 'ipconfig' on Windows
UDP_PORT = 5005

signals = [
    (0, 1000, "sine", 600),
    (1, 1000, "square", 600),
    (2, 1000, "triangle", 600),
    (3, 1000, "sawtooth", 600),
]

print("=== Generator with UDP Sync ===")
for wave, freq, name, duration in signals:
    print(f"\n[{time.strftime('%H:%M:%S')}] Starting {name}")
    
    # Send label over network
    label_msg = f"LABEL:{wave}:{name}".encode()
    sock.sendto(label_msg, (WINDOWS_IP, UDP_PORT))
    print(f"  Sent label to {WINDOWS_IP}:{UDP_PORT}")
    
    # Configure generator
    gen.write(f"config {wave} {freq}\r\n".encode())
    time.sleep(0.5)
    gen.write(b"start\r\n")
    
    time.sleep(duration)
    gen.write(b"stop\r\n")
    time.sleep(2)

gen.close()
sock.close()