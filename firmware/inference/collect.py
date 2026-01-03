import socket
import sys
import datetime

# Configuration
ESP32_IP = "192.168.1.151"  # Change to your ESP32's IP address
PORT = 3333
OUTPUT_FILE = f"dataset_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

print(f"ESP32 WiFi Data Collector")
print(f"=" * 50)
print(f"Connecting to {ESP32_IP}:{PORT}...")

try:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(10)  # 10 second timeout for connection
        sock.connect((ESP32_IP, PORT))
        sock.settimeout(None)  # No timeout after connected
        
        print(f"✓ Connected successfully!")
        print(f"✓ Saving data to: {OUTPUT_FILE}")
        print(f"\nReceiving data... Press Ctrl+C to stop\n")
        
        total_windows = 0
        
        with open(OUTPUT_FILE, 'wb') as f:
            while True:
                data = sock.recv(8192)
                if not data:
                    print("\nConnection closed by ESP32")
                    break
                    
                f.write(data)
                f.flush()
                
                # Count windows (lines)
                lines = data.count(b'\n')
                if lines > 0:
                    total_windows += lines
                    print(f"Windows received: {total_windows}", end='\r')
                    
except KeyboardInterrupt:
    print(f"\n\n{'='*50}")
    print(f"Data collection stopped by user")
    print(f"Total windows collected: {total_windows}")
    print(f"File saved: {OUTPUT_FILE}")
    print(f"{'='*50}")
    
except socket.timeout:
    print(f"\n✗ Connection timeout. Is ESP32 IP correct?")
    print(f"  Check ESP32 serial monitor for actual IP address")
    sys.exit(1)
    
except ConnectionRefusedError:
    print(f"\n✗ Connection refused. Possible issues:")
    print(f"  1. ESP32 is not connected to WiFi")
    print(f"  2. Wrong IP address")
    print(f"  3. TCP server not started on ESP32")
    print(f"  Check ESP32 serial monitor for status")
    sys.exit(1)
    
except Exception as e:
    print(f"\n✗ Error: {e}")
    sys.exit(1)