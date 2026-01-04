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

with open(output_file, 'w') as f:
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