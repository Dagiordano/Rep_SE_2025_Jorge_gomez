import cv2
import numpy as np
import os
from datetime import datetime
import requests
import time
import socket
import io
from PIL import Image

# ESP32-CAM configuration
ESP32_IP = "192.168.1.100"  # Replace with your ESP32-CAM IP address
ESP32_PORT = 80  # Default HTTP port

def get_image_from_esp32():
    """Capture image from ESP32-CAM"""
    try:
        # Try HTTP method first
        url = f"http://{ESP32_IP}/capture"
        response = requests.get(url, timeout=10)
        if response.status_code == 200:
            # Convert the response content to a numpy array
            image_array = np.array(Image.open(io.BytesIO(response.content)))
            # Convert from RGB to BGR (OpenCV format)
            return cv2.cvtColor(image_array, cv2.COLOR_RGB2BGR)
    except requests.RequestException as e:
        print(f"HTTP request failed: {e}")
    
    try:
        # Fallback to socket connection
        print("Trying socket connection...")
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((ESP32_IP, ESP32_PORT))
            s.sendall(b"GET /capture HTTP/1.1\r\nHost: " + ESP32_IP.encode() + b"\r\n\r\n")
            
            # Receive the response
            data = b""
            while True:
                chunk = s.recv(4096)
                if not chunk:
                    break
                data += chunk
                
                # Check if we've received the complete image
                if b"\r\n\r\n" in data and b"\r\n0\r\n\r\n" in data:
                    break
            
            # Extract image data from HTTP response
            parts = data.split(b"\r\n\r\n", 1)
            if len(parts) > 1:
                image_data = parts[1]
                # Handle chunked encoding if present
                if b"Transfer-Encoding: chunked" in parts[0]:
                    # Simple chunked decoding (not full implementation)
                    decoded = b""
                    chunks = image_data.split(b"\r\n")
                    i = 0
                    while i < len(chunks):
                        try:
                            size = int(chunks[i].decode('ascii'), 16)
                            if size == 0:
                                break
                            decoded += chunks[i+1][:size]
                            i += 2
                        except Exception as e:
                            print(f"Chunk decoding error: {e}")
                            break
                    image_data = decoded
                
                # Convert to image
                image_array = np.array(Image.open(io.BytesIO(image_data)))
                return cv2.cvtColor(image_array, cv2.COLOR_RGB2BGR)
    except Exception as e:
        print(f"Socket connection failed: {e}")
    
    print("Failed to get image from ESP32-CAM")
    return None

def apply_histogram_equalization(image):
    """Apply histogram equalization to the image."""
    if len(image.shape) == 3:  # Color image
        # Convert to YUV
        img_yuv = cv2.cvtColor(image, cv2.COLOR_BGR2YUV)
        # Equalize the Y channel
        img_yuv[:,:,0] = cv2.equalizeHist(img_yuv[:,:,0])
        # Convert back to BGR
        return cv2.cvtColor(img_yuv, cv2.COLOR_YUV2BGR)
    else:  # Grayscale image
        return cv2.equalizeHist(image)

def apply_sobel(image):
    """Apply Sobel edge detection to the image."""
    # Convert to grayscale if color
    if len(image.shape) == 3:
        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    else:
        gray = image
    
    # Apply Sobel operator
    sobel_x = cv2.Sobel(gray, cv2.CV_64F, 1, 0, ksize=3)
    sobel_y = cv2.Sobel(gray, cv2.CV_64F, 0, 1, ksize=3)
    
    # Compute the magnitude
    sobel = cv2.magnitude(sobel_x, sobel_y)
    
    # Normalize to 0-255
    sobel = cv2.normalize(sobel, None, 0, 255, cv2.NORM_MINMAX, cv2.CV_8U)
    
    return sobel

def main():
    # Create output directory if it doesn't exist
    output_dir = "esp32_processed_images"
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    print("Connecting to ESP32-CAM...")
    print(f"IP address: {ESP32_IP}")
    
    images_captured = 0
    max_images = 5
    
    while images_captured < max_images:
        print(f"Capturing image {images_captured+1}/{max_images}...")
        
        # Get frame from ESP32-CAM
        frame = get_image_from_esp32()
        
        if frame is None:
            print("Failed to capture image. Retrying in 2 seconds...")
            time.sleep(2)
            continue
        
        print("Image captured successfully.")
        
        # Apply histogram equalization
        hist_eq = apply_histogram_equalization(frame)
        
        # Apply Sobel edge detection
        sobel = apply_sobel(frame)
        
        # Create a timestamp for unique filenames
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        
        # Save original image
        original_path = os.path.join(output_dir, f"original_{images_captured+1}_{timestamp}.jpg")
        cv2.imwrite(original_path, frame)
        
        # Save histogram equalized image
        hist_path = os.path.join(output_dir, f"histogram_{images_captured+1}_{timestamp}.jpg")
        cv2.imwrite(hist_path, hist_eq)
        
        # Save Sobel image
        sobel_path = os.path.join(output_dir, f"sobel_{images_captured+1}_{timestamp}.jpg")
        cv2.imwrite(sobel_path, sobel)
        
        # Display the images
        cv2.imshow('Original', frame)
        cv2.imshow('Histogram Equalization', hist_eq)
        cv2.imshow('Sobel Edge Detection', sobel)
        
        print(f"Saved image set {images_captured+1}/{max_images}")
        
        # Increment counter
        images_captured += 1
        
        # Wait 2 seconds between captures, or press 'q' to quit early
        key = cv2.waitKey(2000)
        if key == ord('q'):
            break
    
    # Close windows
    cv2.destroyAllWindows()
    
    print(f"Successfully captured and saved {images_captured} sets of images to '{output_dir}' directory.")

if __name__ == "__main__":
    main() 