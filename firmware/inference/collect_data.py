import serial
import time
import numpy as np
import pandas as pd
import json
from datetime import datetime
from pathlib import Path
import argparse
from collections import defaultdict

class ESP32DataCollector:
    def __init__(self, port='COM3', baudrate=115200, 
                 sample_window=256, sampling_rate=20000):
        """
        Initialize ESP32 data collector
        
        Args:
            port: Serial port (COM3, /dev/ttyUSB0, etc.)
            baudrate: Baud rate (default 115200)
            sample_window: Number of samples per window
            sampling_rate: Sampling rate in Hz
        """
        self.port = port
        self.baudrate = baudrate
        self.sample_window = sample_window
        self.sampling_rate = sampling_rate
        self.serial_conn = None
        self.current_label = None
        self.waveforms = ['SINE', 'SQUARE', 'TRIANGLE', 'SAWTOOTH']
        
        # Create data directory
        self.data_dir = Path("esp32_collected_data")
        self.csv_dir = self.data_dir / "waveform_csv"
        self.csv_dir.mkdir(parents=True, exist_ok=True)
        
        # Initialize data storage for each waveform (ML format only)
        self.waveform_data_ml = {waveform: [] for waveform in self.waveforms}
        
        print(f"Data will be saved to: {self.data_dir}")
        print(f"Target waveforms: {self.waveforms}")
        print(f"Each waveform will have 1 ML-friendly CSV file")
        print(f"Total: 4 waveform files + 1 combined file = 5 CSV files")
    
    def connect(self):
        """Connect to ESP32 via serial"""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=2.0
            )
            
            # Clear any existing data
            self.serial_conn.reset_input_buffer()
            self.serial_conn.reset_output_buffer()
            time.sleep(2)
            print(f"Connected to ESP32 on {self.port}")
            return True
            
        except serial.SerialException as e:
            print(f"Failed to connect to {self.port}: {e}")
            return False
    
    def send_label(self, label):
        """
        Send label to ESP32
        
        Args:
            label: Waveform label (SINE, SQUARE, TRIANGLE, SAWTOOTH)
        """
        if label.upper() not in self.waveforms:
            print(f"Warning: Label '{label}' not in expected waveforms")
        
        label_str = f"LBL:{label}\n"
        self.serial_conn.write(label_str.encode())
        self.serial_conn.flush()
        self.current_label = label.upper()
        print(f"Label sent: {label}")
        time.sleep(0.5)
    
    def collect_samples_for_waveform(self, waveform, samples_per_class=100):
        """
        Collect training samples for a specific waveform from ESP32
        
        Args:
            waveform: Waveform to collect (SINE, SQUARE, TRIANGLE, SAWTOOTH)
            samples_per_class: Number of sample windows (256 samples each) to collect for this waveform
        """
        if not self.serial_conn:
            print("Not connected to ESP32")
            return
        
        print(f"\nCollecting {samples_per_class} sample windows for {waveform}")
        print(f"Each window has {self.sample_window} samples")
        print(f"Total samples for {waveform}: {samples_per_class * self.sample_window}")
        
        collecting_state = 'waiting'
        current_samples = []
        collected_windows = 0
        
        try:
            # Clear buffers
            self.serial_conn.reset_input_buffer()
            time.sleep(0.5)
            
            print(f"Waiting for ESP32 to start sending data for {waveform}...")
            
            while collected_windows < samples_per_class:
                
                if self.serial_conn.in_waiting:
                    try:
                        line = self.serial_conn.readline().decode('utf-8', errors='ignore').strip()
                        
                        if not line:
                            continue
                        
                        # Check for state markers
                        if '===ADC_START===' in line:
                            collecting_state = 'collecting'
                            current_samples = []
                            
                        elif '===ADC_END===' in line:
                            collecting_state = 'waiting'
                            
                            # Check if we got the right number of samples
                            if len(current_samples) == self.sample_window and self.current_label:
                                # Store in ML format (list of samples for one window)
                                self.waveform_data_ml[self.current_label].append(current_samples)
                                
                                collected_windows += 1
                                
                                # Progress update
                                if collected_windows % 10 == 0 or collected_windows == samples_per_class:
                                    self._print_waveform_progress(waveform, collected_windows, samples_per_class)
                                
                                # Show first sample info
                                if collected_windows == 1:
                                    print(f"\nFirst sample window collected for {waveform}:")
                                    print(f"  Samples in window: {len(current_samples)}")
                            elif len(current_samples) > 0:
                                print(f"Warning: Got {len(current_samples)} samples, expected {self.sample_window}")
                            
                        elif collecting_state == 'collecting':
                            # Try to parse as ADC sample
                            try:
                                sample = int(line.strip())
                                current_samples.append(sample)
                            except ValueError:
                                pass  # Not a sample
                    
                    except Exception as e:
                        print(f"Error parsing line: {e}")
                        continue
                
                else:
                    # No data available
                    time.sleep(0.01)
            
            print(f"\n✓ Successfully collected {collected_windows} windows for {waveform}")
                
        except KeyboardInterrupt:
            print(f"\n\nCollection for {waveform} interrupted by user")
            return collected_windows
        
        return collected_windows
    
    def _save_ml_csv_files(self):
        """Save each waveform's data to ML-friendly CSV files"""
        print("\n" + "="*50)
        print("Saving waveform data to ML-friendly CSV files...")
        
        for waveform in self.waveforms:
            data = self.waveform_data_ml[waveform]
            
            if not data:
                print(f"  {waveform}: No data collected")
                continue
            
            # Create ML-friendly DataFrame (one row per window)
            ml_rows = []
            for window_idx, window_samples in enumerate(data):
                row_data = {'label': waveform, 'window_index': window_idx}
                # Add each sample as a separate column
                for i in range(min(len(window_samples), self.sample_window)):
                    row_data[f'sample_{i}'] = window_samples[i]
                ml_rows.append(row_data)
            
            ml_df = pd.DataFrame(ml_rows)
            
            # Save to CSV (only ML format)
            csv_filename = f"{waveform}_ml.csv"
            csv_path = self.csv_dir / csv_filename
            ml_df.to_csv(csv_path, index=False)
            
            print(f"  {waveform}: {len(data)} windows → {csv_filename}")
    
    def _save_collection_summary(self):
        """Save collection summary"""
        stats = {
            waveform: len(self.waveform_data_ml[waveform]) 
            for waveform in self.waveforms
        }
        total_windows = sum(stats.values())
        
        summary = {
            'collection_time': datetime.now().isoformat(),
            'sample_window': self.sample_window,
            'sampling_rate': self.sampling_rate,
            'waveforms': self.waveforms,
            'windows_collected': stats,
            'total_windows': total_windows,
            'total_samples': total_windows * self.sample_window,
            'ml_files': [f"{waveform}_ml.csv" for waveform in self.waveforms if stats.get(waveform, 0) > 0],
            'combined_file': "all_waveforms_combined.csv",
            'total_csv_files': 5,
            'data_directory': str(self.csv_dir)
        }
        
        summary_file = self.data_dir / f"collection_summary_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        with open(summary_file, 'w') as f:
            json.dump(summary, f, indent=2)
        
        print(f"\nSummary saved to: {summary_file}")
    
    def _print_waveform_progress(self, waveform, collected, target):
        """Print collection progress for a specific waveform"""
        progress = collected / target * 100
        samples = collected * self.sample_window
        print(f"  {waveform}: {collected}/{target} windows ({samples} samples) ({progress:.1f}%)")
    
    def combine_all_waveforms(self):
        """Combine all 4 waveform CSV files into a single training dataset"""
        ml_files = list(self.csv_dir.glob("*_ml.csv"))
        
        if len(ml_files) != 4:
            print(f"Warning: Found {len(ml_files)} ML files, expected 4")
            if not ml_files:
                print("No ML CSV files found")
                return
        
        all_data = []
        for ml_file in ml_files:
            df = pd.read_csv(ml_file)
            all_data.append(df)
        
        if all_data:
            combined_df = pd.concat(all_data, ignore_index=True)
            combined_path = self.data_dir / "all_waveforms_combined.csv"
            combined_df.to_csv(combined_path, index=False)
            
            print(f"\nCombined {len(ml_files)} waveform files into: {combined_path}")
            print(f"Total rows: {len(combined_df)}")
            print("\nWaveform distribution:")
            print(combined_df['label'].value_counts())
            
            print(f"\nTotal CSV files created: 5")
            print("  1. SINE_ml.csv")
            print("  2. SQUARE_ml.csv")
            print("  3. TRIANGLE_ml.csv")
            print("  4. SAWTOOTH_ml.csv")
            print("  5. all_waveforms_combined.csv")
    
    def list_collected_samples(self):
        """List all collected samples"""
        ml_files = list(self.csv_dir.glob("*_ml.csv"))
        
        if not ml_files:
            print("No ML CSV files found")
            return
        
        print(f"\nCollected waveform data (ML format):")
        total_windows = 0
        for waveform in self.waveforms:
            ml_file = self.csv_dir / f"{waveform}_ml.csv"
            if ml_file.exists():
                df = pd.read_csv(ml_file)
                windows = len(df)
                total_windows += windows
                print(f"  {waveform}: {windows} windows")
            else:
                print(f"  {waveform}: Not collected")
        
        # Check for combined file
        combined_file = self.data_dir / "all_waveforms_combined.csv"
        if combined_file.exists():
            combined_df = pd.read_csv(combined_file)
            print(f"\nCombined dataset: {len(combined_df)} windows")
        
        print(f"\nTotal CSV files: {len(ml_files) + (1 if combined_file.exists() else 0)}")
    
    def disconnect(self):
        """Disconnect from ESP32"""
        if self.serial_conn:
            self.serial_conn.close()
            print("Disconnected from ESP32")

def main():
    parser = argparse.ArgumentParser(description='ESP32 Data Collector - ML-friendly format only (5 CSV files)')
    parser.add_argument('--port', type=str, default='COM3',
                       help='Serial port (COM3, /dev/ttyUSB0, etc.)')
    parser.add_argument('--samples', type=int, default=100,
                       help='Sample windows per waveform to collect (each window = 256 samples)')
    parser.add_argument('--list', action='store_true',
                       help='List collected waveforms without collecting new data')
    parser.add_argument('--combine', action='store_true',
                       help='Combine all 4 waveform CSV files into training dataset')
    
    args = parser.parse_args()
    
    # Initialize collector
    collector = ESP32DataCollector(
        port=args.port,
        baudrate=115200
    )
    
    if args.list:
        collector.list_collected_samples()
        return
    
    if args.combine:
        collector.combine_all_waveforms()
        return
    
    # Connect and collect data
    if collector.connect():
        try:
            print("\n" + "="*60)
            print("ESP32 WAVEFORM DATA COLLECTOR - ML FORMAT ONLY")
            print("="*60)
            print(f"\nWill collect {args.samples} windows per waveform")
            print(f"Each window = {collector.sample_window} samples")
            print(f"Total per waveform: {args.samples * collector.sample_window} samples")
            print(f"\nThis will create 5 CSV files total:")
            print("  1. SINE_ml.csv")
            print("  2. SQUARE_ml.csv")
            print("  3. TRIANGLE_ml.csv")
            print("  4. SAWTOOTH_ml.csv")
            print("  5. all_waveforms_combined.csv (auto-created)")
            print("\nInstructions:")
            print("1. Connect signal generator to ESP32")
            print("2. Set generator to one of the 4 waveforms")
            print("3. Press Enter to start collection for that waveform")
            print("4. Collection stops automatically after specified number of windows")
            print("5. Switch waveform and press Enter to collect next waveform")
            print("\nPress Ctrl+C at any time to stop collection")
            print("="*60)
            
            # Manual collection mode - one waveform at a time
            for waveform in collector.waveforms:
                input(f"\nPress Enter to collect {args.samples} windows of {waveform}...")
                
                # Send label to ESP32
                collector.send_label(waveform)
                
                # Collect samples for this specific waveform
                collected = collector.collect_samples_for_waveform(
                    waveform=waveform,
                    samples_per_class=args.samples
                )
                
                if collected < args.samples:
                    print(f"\nWarning: Only collected {collected} out of {args.samples} windows for {waveform}")
                    continue_option = input("Continue to next waveform? (y/n): ")
                    if continue_option.lower() != 'y':
                        break
            
            # Save collected data to CSV files
            print("\n" + "="*50)
            print("Saving collected data to CSV files...")
            collector._save_ml_csv_files()
            
            # Auto-combine data
            print("\n" + "="*50)
            print("Auto-combining all waveforms into single dataset...")
            collector.combine_all_waveforms()
            
            # Save summary
            collector._save_collection_summary()
            
            # List collected samples
            collector.list_collected_samples()
                
        except KeyboardInterrupt:
            print("\n\nCollection stopped by user")
            # Save any data collected so far
            if any(len(collector.waveform_data_ml[w]) > 0 for w in collector.waveforms):
                save_option = input("\nSave collected data before exiting? (y/n): ")
                if save_option.lower() == 'y':
                    collector._save_ml_csv_files()
                    collector._save_collection_summary()
        finally:
            collector.disconnect()
    else:
        print("Failed to connect to ESP32")

if __name__ == "__main__":
    main()