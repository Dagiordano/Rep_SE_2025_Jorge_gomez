import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import numpy as np
from collections import deque

# Configure serial port
SERIAL_PORT = '/dev/cu.usbserial-110'  # Change this to match your port
BAUD_RATE = 115200

# Data storage
MAX_POINTS = 1000  # Number of points to display
ecg_data = deque([0] * MAX_POINTS, maxlen=MAX_POINTS)
pressure_data = deque([0] * MAX_POINTS, maxlen=MAX_POINTS)
time_data = deque(np.linspace(0, MAX_POINTS/100, MAX_POINTS), maxlen=MAX_POINTS)

# Create figure and subplots
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))
fig.suptitle('ECG and Pressure Monitoring')

# Configure plots
ax1.set_title('ECG Signal')
ax1.set_ylabel('Voltage (V)')
ax1.set_ylim(-0.5, 3.5)
line1, = ax1.plot([], [], 'b-', label='ECG')

ax2.set_title('Pressure Signal')
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('Voltage (V)')
ax2.set_ylim(-0.5, 3.5)
line2, = ax2.plot([], [], 'r-', label='Pressure')

# Add legends
ax1.legend()
ax2.legend()

# Adjust layout
plt.tight_layout()

# Initialize serial connection
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE)
except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
    exit(1)

def update(frame):
    if ser.in_waiting:
        try:
            line = ser.readline().decode('utf-8').strip()
            if line.startswith('ECG:') and 'PRESSURE:' in line:
                # Parse the data
                ecg_str, pressure_str = line.split(',')
                ecg = float(ecg_str.split(':')[1])
                pressure = float(pressure_str.split(':')[1])
                
                # Update data
                ecg_data.append(ecg)
                pressure_data.append(pressure)
                time_data.append(time_data[-1] + 0.01)  # 10ms intervals
                
                # Update plots
                line1.set_data(list(time_data), list(ecg_data))
                line2.set_data(list(time_data), list(pressure_data))
                
                # Adjust x-axis limits to show last 10 seconds
                current_time = time_data[-1]
                ax1.set_xlim(current_time - 10, current_time)
                ax2.set_xlim(current_time - 10, current_time)
        except (ValueError, IndexError) as e:
            print(f"Error parsing data: {e}")
    
    return line1, line2

# Create animation
ani = FuncAnimation(fig, update, interval=10, blit=True)

try:
    plt.show()
except KeyboardInterrupt:
    print("\nStopping...")
finally:
    ser.close() 