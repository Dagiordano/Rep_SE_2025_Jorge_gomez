import serial
import time

def read_spectrogram(port='/dev/cu.usbserial-110', baudrate=115200, timeout=10):
    """Read spectrogram data from ESP32 serial output"""
    print(f"Attempting to connect to ESP32 on port {port}...")
    ser = serial.Serial(port, baudrate, timeout=1)
    
    data = []
    start_marker_found = False
    timeout_start = time.time()
    
    try:
        while (time.time() - timeout_start) < timeout:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8').strip()
                timeout_start = time.time()  # Reset timeout when we get data
                
                if line == "===SPECTROGRAM_START===":
                    print("Found start marker, reading data...")
                    start_marker_found = True
                    continue
                    
                if start_marker_found:
                    if line == "===SPECTROGRAM_END===":
                        print("Found end marker, finished reading")
                        break
                    try:
                        value = float(line)
                        data.append(value)
                    except ValueError:
                        continue
                        
        if data:
            print(f"Successfully read {len(data)} values")
            # Save to file
            with open('spectrogram.txt', 'w') as f:
                for value in data:
                    f.write(f"{value}\n")
            print("Data saved to spectrogram.txt")
        else:
            print("No data was read")
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        ser.close()
    
    return data

if __name__ == "__main__":
    try:
        data = read_spectrogram()
    except serial.SerialException as e:
        print(f"\nError: Could not open serial port. Please check:")
        print("1. The ESP32 is connected")
        print("2. The correct port is being used")
        print("3. You have permissions to access the port")
        print(f"\nTechnical details: {e}")