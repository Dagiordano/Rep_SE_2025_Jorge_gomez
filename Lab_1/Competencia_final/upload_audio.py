import os
import sys
import subprocess
import argparse

def upload_audio_file(port, audio_file):
    # Check if audio file exists
    if not os.path.exists(audio_file):
        print(f"Error: Audio file {audio_file} not found")
        return False

    # Create a temporary directory for SPIFFS image
    temp_dir = "spiffs_image"
    os.makedirs(temp_dir, exist_ok=True)

    # Copy audio file to SPIFFS image directory
    os.system(f"cp {audio_file} {temp_dir}/audio.wav")

    # Get ESP-IDF path
    idf_path = os.environ.get('IDF_PATH')
    if not idf_path:
        print("Error: IDF_PATH environment variable not set")
        return False

    # Generate SPIFFS image
    spiffsgen_path = os.path.join(idf_path, "components", "spiffs", "spiffsgen.py")
    partition_size = "0x100000"  # 1MB
    spiffs_image = "spiffs.bin"

    cmd = [
        "python",
        spiffsgen_path,
        partition_size,
        temp_dir,
        spiffs_image
    ]

    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError:
        print("Error generating SPIFFS image")
        return False

    # Flash SPIFFS image
    esptool_cmd = [
        "esptool.py",
        "--port", port,
        "--baud", "921600",
        "write_flash",
        "0x110000",  # Offset for storage partition
        spiffs_image
    ]

    try:
        subprocess.run(esptool_cmd, check=True)
    except subprocess.CalledProcessError:
        print("Error flashing SPIFFS image")
        return False

    # Clean up
    os.remove(spiffs_image)
    os.system(f"rm -rf {temp_dir}")

    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Upload audio file to ESP32 SPIFFS')
    parser.add_argument('--port', required=True, help='Serial port (e.g., /dev/ttyUSB0)')
    parser.add_argument('--audio', required=True, help='Path to audio file')
    
    args = parser.parse_args()
    
    if upload_audio_file(args.port, args.audio):
        print("Audio file uploaded successfully!")
    else:
        print("Failed to upload audio file")
        sys.exit(1) 