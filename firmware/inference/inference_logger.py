import serial
import socket
import threading
import time

inf = serial.Serial('COM7', 115200, timeout=1)
output_file = f"dataset_{int(time.time())}.csv"

# UDP listener
current_label = 0
label_name = "unknown"
last_printed_label = -1  # Initialize tracking variable

def udp_listener():
    global current_label, label_name
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', 5005))
    print("UDP listener started on port 5005")
    
    while True:
        data, addr = sock.recvfrom(1024)
        msg = data.decode()
        if msg.startswith("LABEL:"):
            parts = msg.split(':')
            current_label = int(parts[1])
            label_name = parts[2]
            print(f"\n[SYNC] Label updated: {label_name} (wave={current_label})")

# Start UDP listener in background
listener_thread = threading.Thread(target=udp_listener, daemon=True)
listener_thread.start()

print(f"Logging to {output_file}")
print("Waiting for labels over UDP...\n")

# Generate column headers based on your feature extraction
def generate_headers():
    # Fixed columns at the beginning
    headers = ["Type", "timestamp_us", "window_id", "label", "sample_rate_hz"]
    
    # Feature columns (16 features from your code)
    for i in range(16):
        headers.append(f"feature_{i}")
    
    # Inference result columns
    headers.extend(["predicted_type", "confidence"])
    
    # Raw sample columns (WINDOW_SIZE samples)
    for i in range(256):  # WINDOW_SIZE from your code
        headers.append(f"sample_{i}")
    
    return headers

with open(output_file, 'w') as f:
    # Write headers first
    headers = generate_headers()
    f.write(','.join(headers) + '\n')
    f.flush()
    print(f"CSV headers written: {len(headers)} columns")
    
    try:
        while True:
            line = inf.readline().decode('utf-8', errors='ignore').strip()
            if line.startswith("ML_DATA"):
                # Inject current label into data
                parts = line.split(',')
                if len(parts) > 3:
                    parts[3] = str(current_label)  # Replace label column
                    f.write(','.join(parts) + '\n')
                    f.flush()
                    
                    # DEBUG: Print first few values when label changes
                    if int(current_label) != last_printed_label:
                        print(f"\n[{label_name}] First few samples: {parts[4:10]}")
                        last_printed_label = current_label
                    
                    print(f"[{label_name}] ", end='', flush=True)
    except KeyboardInterrupt:
        print(f"\n\nSaved to {output_file}")

inf.close()