import serial
import time
import socket
import sys

# Configure serial port
try:
    gen = serial.Serial(
        port='/dev/ttyUSB0',
        baudrate=115200,
        timeout=1,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False
    )
except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
    sys.exit(1)

# Configure UDP socket for sync
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
WINDOWS_IP = "192.168.1.120"  # Find with 'ipconfig' on Windows
UDP_PORT = 5005

# Updated signal parameters for better quality
signals = [
    (0, 500,  "sine",      60),  # Higher frequency OK for sine
    (1, 50,   "square",    60),  # Lower frequency for square
    (2, 100,  "triangle",  60),  # Moderate frequency for triangle
    (3, 30,   "sawtooth",  60),  # Low frequency for sawtooth
]

def clean_serial_buffer():
    """Clear any pending data in the serial buffer"""
    time.sleep(0.1)
    while gen.in_waiting:
        try:
            data = gen.read(gen.in_waiting)
            # Try to decode, but ignore if it fails
            try:
                decoded = data.decode('utf-8', errors='ignore').strip()
                if decoded:
                    print(f"  ESP (clearing): {decoded}")
            except:
                pass
        except:
            pass
    time.sleep(0.1)

def read_serial_response(timeout=1.0):
    """Read serial response with proper decoding"""
    response = ""
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        if gen.in_waiting:
            try:
                line = gen.readline()
                # Skip empty lines
                if not line:
                    continue
                    
                try:
                    decoded = line.decode('utf-8', errors='replace').strip()
                    if decoded:
                        response += decoded + "\n"
                        print(f"  ESP: {decoded}")
                except Exception as e:
                    print(f"  ESP (binary data): {line.hex()[:50]}...")
                    
            except Exception as e:
                print(f"  Serial read error: {e}")
                break
        else:
            time.sleep(0.01)
            
    return response

print("=== Generator with UDP Sync ===")
print("Using improved signal parameters for clean output")
print("Add RC filter (1kΩ + 100nF to GND) for best results\n")

# Clear any initial garbage
print("Clearing serial buffer...")
clean_serial_buffer()

for wave, freq, name, duration in signals:
    print(f"\n{'='*50}")
    print(f"[{time.strftime('%H:%M:%S')}] Starting {name} at {freq} Hz")
    print(f"{'='*50}")
    
    # Send label over network
    label_msg = f"LABEL:{wave}:{name}:{freq}".encode()
    try:
        sock.sendto(label_msg, (WINDOWS_IP, UDP_PORT))
        print(f"  Sent label to {WINDOWS_IP}:{UDP_PORT}")
    except Exception as e:
        print(f"  UDP send error: {e}")
    
    # Clear buffer before sending command
    clean_serial_buffer()
    
    # Configure generator
    config_cmd = f"config {wave} {freq}\r\n"
    print(f"  Sending: {config_cmd.strip()}")
    gen.write(config_cmd.encode())
    gen.flush()  # Ensure data is sent
    
    # Wait for and read response
    print("  Waiting for response...")
    response = read_serial_response(timeout=2.0)
    
    # Check if configuration was successful
    if "Configuration updated" not in response:
        print(f"  WARNING: Configuration may have failed!")
        print(f"  Response: {response[:100]}...")
    
    # Clear buffer before starting
    clean_serial_buffer()
    
    # Start generation
    print("  Starting generation...")
    gen.write(b"start\r\n")
    gen.flush()
    
    # Read startup confirmation
    response = read_serial_response(timeout=1.0)
    if "Starting signal generation" not in response:
        print(f"  WARNING: Start command may have failed!")
    
    # Wait for duration
    print(f"  Running for {duration} seconds...")
    for i in range(duration):
        time.sleep(1)
        # Periodically check for errors
        if gen.in_waiting:
            try:
                data = gen.read(gen.in_waiting)
                try:
                    decoded = data.decode('utf-8', errors='ignore').strip()
                    if "error" in decoded.lower() or "fail" in decoded.lower():
                        print(f"    WARNING at {i}s: {decoded[:50]}...")
                except:
                    pass
            except:
                pass
    
    # Stop generation
    print("  Stopping generation...")
    gen.write(b"stop\r\n")
    gen.flush()
    
    # Read stop confirmation
    response = read_serial_response(timeout=1.0)
    
    # Wait before next signal
    print(f"  Pausing for 2 seconds...")
    time.sleep(2)
    
    # Final buffer clean
    clean_serial_buffer()

print(f"\n{'='*50}")
print("=== Test sequence complete ===")
print("All waveforms generated with improved quality")
print("Check ADC data for clean signals")
print(f"{'='*50}")

# Close connections
try:
    gen.close()
    sock.close()
    print("Connections closed.")
except:
    pass