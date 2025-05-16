import serial
import time
import numpy as np
import cv2
import os
from datetime import datetime

# Serial port configuration
SERIAL_PORT = '/dev/cu.usbserial-110'  # Change this to match your ESP32's port
BAUD_RATE = 115200

# Image dimensions (must match ESP32 code)
WIDTH = 640
HEIGHT = 480

def create_output_dirs():
    """Create directories for saving images if they don't exist."""
    base_dir = 'esp32_images'
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    session_dir = os.path.join(base_dir, timestamp)
    
    # Create directories for original, histogram, and sobel images
    dirs = {
        'original': os.path.join(session_dir, 'original'),
        'histogram': os.path.join(session_dir, 'histogram'),
        'sobel': os.path.join(session_dir, 'sobel')
    }
    
    for dir_path in dirs.values():
        os.makedirs(dir_path, exist_ok=True)
    
    return dirs

def receive_image(ser, width, height):
    """Receive a single image from serial port."""
    # Wait for start marker
    while ser.read(1) != b'\xFF':
        pass
    
    # Read image data
    image_data = bytearray()
    bytes_received = 0
    expected_bytes = width * height
    
    while bytes_received < expected_bytes:
        if ser.in_waiting:
            byte = ser.read(1)
            image_data.extend(byte)
            bytes_received += 1
    
    # Convert to numpy array and reshape
    image = np.frombuffer(image_data, dtype=np.uint8)
    image = image.reshape((height, width))
    
    return image

def main():
    # Create output directories
    dirs = create_output_dirs()
    
    # Open serial port
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to {SERIAL_PORT}")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        return
    
    try:
        image_count = 0
        while image_count < 5:  # Save first 5 images
            print(f"\nWaiting for image {image_count + 1}/5...")
            
            # Receive original image
            print("Receiving original image...")
            original = receive_image(ser, WIDTH, HEIGHT)
            
            # Receive histogram equalized image
            print("Receiving histogram equalized image...")
            histogram = receive_image(ser, WIDTH, HEIGHT)
            
            # Receive Sobel edge detected image
            print("Receiving Sobel edge detected image...")
            sobel = receive_image(ser, WIDTH, HEIGHT)
            
            # Save images
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            
            cv2.imwrite(os.path.join(dirs['original'], f'original_{timestamp}.png'), original)
            cv2.imwrite(os.path.join(dirs['histogram'], f'histogram_{timestamp}.png'), histogram)
            cv2.imwrite(os.path.join(dirs['sobel'], f'sobel_{timestamp}.png'), sobel)
            
            print(f"Saved image set {image_count + 1}")
            image_count += 1
            
            # Small delay between images
            time.sleep(0.5)
            
    except KeyboardInterrupt:
        print("\nStopping image capture...")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        ser.close()
        print("Serial port closed")

if __name__ == "__main__":
    main() 