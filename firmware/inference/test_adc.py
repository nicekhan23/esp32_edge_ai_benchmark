import serial
import numpy as np
import matplotlib.pyplot as plt

ser = serial.Serial('COM7', 115200, timeout=1)
print("Reading ADC data...")

samples = []
for _ in range(256):
    line = ser.readline().decode('utf-8', errors='ignore').strip()
    if line.startswith("ML_DATA"):
        parts = line.split(',')
        # Extract first 10 samples
        raw_samples = parts[23:33]  # Raw samples start at column 23
        samples.extend([int(s) for s in raw_samples if s.isdigit()])

ser.close()

samples = np.array(samples)
print(f"Collected {len(samples)} samples")
print(f"Min: {samples.min()}, Max: {samples.max()}, Mean: {samples.mean():.1f}")
print(f"Range: {samples.max() - samples.min()}")

# Plot
plt.figure(figsize=(10, 4))
plt.plot(samples, 'b-', linewidth=0.5)
plt.axhline(y=2048, color='r', linestyle='--', label='Midpoint (2048)')
plt.axhline(y=samples.mean(), color='g', linestyle='--', label=f'Mean ({samples.mean():.1f})')
plt.xlabel('Sample')
plt.ylabel('ADC Value')
plt.title('Raw ADC Samples')
plt.legend()
plt.grid(True)
plt.savefig('adc_debug.png')
print("Plot saved to adc_debug.png")