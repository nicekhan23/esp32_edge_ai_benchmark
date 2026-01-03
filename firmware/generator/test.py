import serial
import time

# Connect to generator
gen = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

# Test signals
test_cases = [
    (0, 1000, "SINE"),      # wave=0, freq=1000Hz
    (1, 1000, "SQUARE"),    # wave=1, freq=1000Hz
    (2, 1000, "TRIANGLE"),  # wave=2, freq=1000Hz
    (3, 1000, "SAWTOOTH"),  # wave=3, freq=1000Hz
]

for wave, freq, name in test_cases:
    print(f"\n=== Testing {name} @ {freq}Hz ===")
    gen.write(f"config {wave} {freq}\r\n".encode())
    time.sleep(0.5)
    gen.write(b"start\r\n")
    time.sleep(5)  # Collect 5 seconds of data
    gen.write(b"stop\r\n")
    time.sleep(1)

gen.close()