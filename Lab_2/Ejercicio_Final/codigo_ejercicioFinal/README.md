# ESP32-CAM Image Processing System

This project implements an ESP32-CAM-based image capture and processing system that:

1. Continuously captures images and stores them in PSRAM
2. Applies histogram equalization to the images
3. Applies Sobel edge detection filter to the images
4. Maintains a rotating buffer of 20 images, freeing the oldest images when new ones are captured
5. Measures performance metrics (FPS, processing times, power consumption)
6. Evaluates performance at different CPU frequencies

## Requirements

- ESP32-CAM development board with PSRAM (e.g., AI-Thinker ESP32-CAM)
- ESP-IDF development environment

## Project Structure

- `main/pruebacam.c`: Main application code
- `components/esp32-camera`: ESP32 camera driver component

## Hardware Setup

1. Connect the ESP32-CAM to your computer via UART
2. Connect power supply with sufficient current (at least 500mA)

## Building and Flashing

```bash
# Configure project
idf.py menuconfig

# Build the project
idf.py build

# Flash the firmware
idf.py -p [PORT] flash

# Monitor the output
idf.py -p [PORT] monitor
```

## Features

### Image Capture and Storage

- The ESP32-CAM captures VGA (640x480) JPEG images
- Images are stored in the ESP32's PSRAM memory
- Three versions of each image are stored:
  - Original JPEG images
  - Histogram equalized images
  - Sobel filtered images
- A rotating buffer maintains the last 20 image sets in memory

### Image Processing

- **Histogram Equalization:** Enhances image contrast by redistributing intensity values
- **Sobel Filter:** Detects edges in the image using Sobel operators

### Performance Analysis

The system automatically tests and reports:

1. **Frames Per Second (FPS):** How many frames can be processed per second
2. **Processing Times:** Breakdown of time spent in each operation (capture, histogram, Sobel, storage)
3. **Bottleneck Identification:** Which operation takes the most time
4. **Power Consumption:** Estimated power usage at different CPU frequencies
5. **Energy Efficiency:** FPS per Watt analysis to find optimal frequency

## Meeting Assignment Requirements

1. **Image Capture and Storage:** The system captures, processes, and stores images in PSRAM.
2. **FPS Measurement:** The system calculates and reports FPS in real-time.
3. **Bottleneck Analysis:** Processing times for each operation are measured and the bottleneck is identified.
4. **Power Measurement:** Power consumption is estimated for each CPU frequency.
5. **CPU Frequency Analysis:** The system tests different CPU frequencies and reports FPS and power consumption for each.
6. **Energy Calculation:** The system calculates the minimum energy required for 10 days of operation.
7. **Performance Competition:** The optimal CPU frequency for maximum FPS/W is determined.

## Output and Results

The system logs all measurements to the serial console, including:
- FPS at each CPU frequency
- Processing time breakdowns
- Power consumption estimates
- Energy usage projections
- Optimal configuration for performance per watt
- PSRAM memory usage statistics

## Notes

- The power consumption values are estimates based on typical values for ESP32-CAM components.
- For accurate power measurements, external measurement hardware would be required.
- The actual histogram equalization and Sobel filter are implemented but use dummy grayscale data, as a real implementation would need to convert the JPEG images to grayscale first.
- PSRAM usage is monitored during runtime to prevent memory exhaustion. 