#!/usr/bin/env python3
import serial
import struct
import os
import time
from datetime import datetime
import numpy as np
from PIL import Image
import matplotlib.pyplot as plt

# Command definitions (must match the ESP32 code)
CMD_GET_IMAGE = 0x01
CMD_GET_HISTOGRAM = 0x02
CMD_GET_SOBEL = 0x03
CMD_GET_PERFORMANCE = 0x04

# Protocol synchronization
SYNC_HEADER = 0xAA55
SYNC_FOOTER = 0x55AA

# Serial port configuration
SERIAL_PORT = '/dev/tty.usbserial-110'  # Updated port for macOS
BAUD_RATE = 115200
BYTE_SIZE = serial.EIGHTBITS
PARITY = serial.PARITY_NONE
STOP_BITS = serial.STOPBITS_ONE

class ESP32CamRetriever:
    def __init__(self, port=SERIAL_PORT, baudrate=BAUD_RATE):
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=BYTE_SIZE,
            parity=PARITY,
            stopbits=STOP_BITS,
            timeout=1,
            write_timeout=1
        )
        self.output_dir = f"captured_images_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        os.makedirs(self.output_dir, exist_ok=True)
        
    def __del__(self):
        if hasattr(self, 'ser'):
            self.ser.close()

    def send_command(self, cmd, param=None):
        try:
            self.ser.reset_input_buffer()  # Clear any pending input
            self.ser.reset_output_buffer()  # Clear any pending output
            self.ser.write(bytes([cmd]))
            if param is not None:
                self.ser.write(bytes([param]))
            self.ser.flush()  # Ensure all data is sent
            time.sleep(0.1)  # Small delay to ensure command is processed
        except Exception as e:
            print(f"Error sending command: {e}")

    def wait_for_sync(self):
        """Wait for and verify the sync header."""
        while True:
            header_bytes = self.ser.read(2)
            if len(header_bytes) != 2:
                print("Error: Could not read sync header")
                return False
            header = struct.unpack('<H', header_bytes)[0]
            if header == SYNC_HEADER:
                return True
            print(f"Warning: Invalid sync header: 0x{header:04X}")

    def verify_footer(self):
        """Verify the sync footer."""
        footer_bytes = self.ser.read(2)
        if len(footer_bytes) != 2:
            print("Error: Could not read sync footer")
            return False
        footer = struct.unpack('<H', footer_bytes)[0]
        if footer != SYNC_FOOTER:
            print(f"Warning: Invalid sync footer: 0x{footer:04X}")
            return False
        return True

    def read_size(self):
        try:
            if not self.wait_for_sync():
                return 0
                
            size_bytes = self.ser.read(4)
            if len(size_bytes) != 4:
                print(f"Warning: Expected 4 bytes for size, got {len(size_bytes)}")
                return 0
            size = struct.unpack('<I', size_bytes)[0]
            if size > 1000000:  # Sanity check - image shouldn't be larger than 1MB
                print(f"Warning: Suspiciously large size received: {size} bytes")
                return 0
            print(f"Debug: Received size: {size} bytes")
            return size
        except Exception as e:
            print(f"Error reading size: {e}")
            return 0

    def read_image(self):
        size = self.read_size()
        if size == 0:
            print("Warning: Invalid size received")
            return None
        
        data = bytearray()
        start_time = time.time()
        while len(data) < size:
            chunk = self.ser.read(min(size - len(data), 1024))
            if not chunk:
                if time.time() - start_time > 5:  # 5 second timeout
                    print("Timeout while reading image data")
                    break
                continue
            data.extend(chunk)
            print(f"Debug: Read {len(data)}/{size} bytes")
        
        if len(data) != size:
            print(f"Warning: Incomplete data received. Expected {size} bytes, got {len(data)}")
            return None
            
        if not self.verify_footer():
            print("Warning: Invalid footer after image data")
            return None
            
        return data

    def get_performance_metrics(self):
        try:
            self.send_command(CMD_GET_PERFORMANCE)
            print("Debug: Requested performance metrics")
            
            if not self.wait_for_sync():
                return None
                
            # Read and validate FPS
            fps_bytes = self.ser.read(4)
            if len(fps_bytes) != 4:
                print("Error: Could not read FPS value")
                return None
            fps = struct.unpack('<f', fps_bytes)[0]
            if fps < 0 or fps > 1000:  # Sanity check
                print(f"Warning: Suspicious FPS value: {fps}")
                fps = 0
            
            # Read and validate timing values
            times = {}
            for name in ['capture_time', 'histogram_time', 'sobel_time', 'save_time']:
                time_bytes = self.ser.read(8)
                if len(time_bytes) != 8:
                    print(f"Error: Could not read {name}")
                    return None
                time_value = struct.unpack('<Q', time_bytes)[0]
                if time_value > 1000000000:  # Sanity check - shouldn't be more than 1 second
                    print(f"Warning: Suspicious {name}: {time_value} us")
                    time_value = 0
                times[f'{name}_us'] = time_value
            
            if not self.verify_footer():
                return None
                
            return {
                'fps': fps,
                **times
            }
        except Exception as e:
            print(f"Error getting performance metrics: {e}")
            return None

    def save_image(self, data, filename):
        if data is None:
            return
        
        try:
            # Convert raw data to image
            img = Image.frombytes('L', (640, 480), bytes(data))
            img.save(os.path.join(self.output_dir, filename))
        except Exception as e:
            print(f"Error saving image {filename}: {e}")

    def retrieve_all_images(self, num_images=20):
        print(f"Retrieving {num_images} images...")
        
        # Create subdirectories for different image types
        for img_type in ['original', 'histogram', 'sobel']:
            os.makedirs(os.path.join(self.output_dir, img_type), exist_ok=True)

        # Get performance metrics
        perf = self.get_performance_metrics()
        if perf:
            print("\nPerformance Metrics:")
            print(f"FPS: {perf['fps']:.2f}")
            print(f"Capture time: {perf['capture_time_us']/1000:.2f} ms")
            print(f"Histogram time: {perf['histogram_time_us']/1000:.2f} ms")
            print(f"Sobel time: {perf['sobel_time_us']/1000:.2f} ms")
            print(f"Save time: {perf['save_time_us']/1000:.2f} ms")
        else:
            print("Warning: Could not get performance metrics")

        # Retrieve images
        for i in range(num_images):
            print(f"\nRetrieving image {i+1}/{num_images}")
            
            # Get original image
            print("Requesting original image...")
            self.send_command(CMD_GET_IMAGE, i)
            original_data = self.read_image()
            if original_data:
                self.save_image(original_data, f'original/image_{i:02d}.jpg')
                print(f"Saved original image {i}")
            
            # Get histogram image
            print("Requesting histogram image...")
            self.send_command(CMD_GET_HISTOGRAM, i)
            histogram_data = self.read_image()
            if histogram_data:
                self.save_image(histogram_data, f'histogram/image_{i:02d}.jpg')
                print(f"Saved histogram image {i}")
            
            # Get Sobel image
            print("Requesting Sobel image...")
            self.send_command(CMD_GET_SOBEL, i)
            sobel_data = self.read_image()
            if sobel_data:
                self.save_image(sobel_data, f'sobel/image_{i:02d}.jpg')
                print(f"Saved Sobel image {i}")

        print(f"\nImages saved in directory: {self.output_dir}")

    def plot_performance(self):
        perf = self.get_performance_metrics()
        if not perf:
            print("Warning: Could not get performance metrics for plotting")
            return
            
        # Create bar chart
        labels = ['Capture', 'Histogram', 'Sobel', 'Save']
        times = [
            perf['capture_time_us']/1000,
            perf['histogram_time_us']/1000,
            perf['sobel_time_us']/1000,
            perf['save_time_us']/1000
        ]
        
        plt.figure(figsize=(10, 6))
        plt.bar(labels, times)
        plt.title('Processing Times')
        plt.ylabel('Time (ms)')
        plt.savefig(os.path.join(self.output_dir, 'performance.png'))
        plt.close()

def main():
    try:
        retriever = ESP32CamRetriever()
        retriever.retrieve_all_images()
        retriever.plot_performance()
    except serial.SerialException as e:
        print(f"Error: Could not open port {SERIAL_PORT}")
        print(f"Details: {e}")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    main() 