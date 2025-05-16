import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.lines import Line2D
import numpy as np
import time
import argparse

# Parse command line arguments
parser = argparse.ArgumentParser(description='Visualize ECG and FSR data from Arduino')
parser.add_argument('--port', type=str, default='/dev/cu.usbmodem1101', help='Serial port')
parser.add_argument('--baud', type=int, default=9600, help='Baud rate')
args = parser.parse_args()

# Setup serial connection
try:
    ser = serial.Serial(args.port, args.baud)
    print(f"Connected to {args.port} at {args.baud} baud")
except Exception as e:
    print(f"Error connecting to serial port: {e}")
    print("Available ports:")
    import serial.tools.list_ports
    ports = serial.tools.list_ports.comports()
    for port in ports:
        print(f"  {port}")
    exit(1)

# Setup the figure and axes
fig, (ax_ecg, ax_fsr) = plt.subplots(2, 1, figsize=(12, 8))
fig.suptitle('ECG and Pressure Monitoring', fontsize=16)

# Setup ECG plot
ax_ecg.set_ylabel('ECG Signal')
ax_ecg.set_xlabel('Time (s)')
ax_ecg.set_ylim(0, 1023)  # ADC range
ax_ecg.grid(True)

# Setup FSR pressure plot
ax_fsr.set_ylabel('Pressure')
ax_fsr.set_xlabel('Time (s)')
ax_fsr.set_ylim(0, 1023)  # ADC range
ax_fsr.grid(True)

# Create a canvas for plotting and add text for compression rate
compression_rate_text = fig.text(0.02, 0.5, 'Compression Rate: -- BPM', fontsize=12,
                                 bbox=dict(facecolor='white', alpha=0.8))

# Create line objects
time_window = 10  # seconds to display
sample_rate = 50  # estimated samples per second
buffer_size = time_window * sample_rate

# Initialize time array (converted to seconds)
time_array = np.linspace(-time_window, 0, buffer_size)
ecg_data = np.zeros(buffer_size)
fsr_data = np.zeros(buffer_size)

# Create line objects
line_ecg = Line2D(time_array, ecg_data, color='red')
line_fsr = Line2D(time_array, fsr_data, color='blue')

ax_ecg.add_line(line_ecg)
ax_fsr.add_line(line_fsr)

# Create lead off indicator
lead_off_text = fig.text(0.02, 0.95, '', fontsize=12, color='red',
                         bbox=dict(facecolor='white', alpha=0.8))

# Create pressure class indicator
pressure_text = fig.text(0.02, 0.45, 'Pressure: --', fontsize=12,
                         bbox=dict(facecolor='white', alpha=0.8))

# Variables to hold the latest data
latest_compression_rate = "--"
latest_pressure_class = "--"
lead_off_status = False

def init():
    """Initialize the animation"""
    line_ecg.set_data(time_array, ecg_data)
    line_fsr.set_data(time_array, fsr_data)
    return line_ecg, line_fsr

def update_plot(frame):
    """Update the plot with new data"""
    global ecg_data, fsr_data, latest_compression_rate, latest_pressure_class, lead_off_status
    
    # Update displayed text
    compression_rate_text.set_text(f'Compression Rate: {latest_compression_rate} BPM')
    pressure_text.set_text(f'Pressure: {latest_pressure_class}')
    
    if lead_off_status:
        lead_off_text.set_text('ECG LEADS OFF!')
    else:
        lead_off_text.set_text('')
    
    # Shift old data to the left
    ecg_data = np.roll(ecg_data, -1)
    fsr_data = np.roll(fsr_data, -1)
    
    # Get new data if available
    while ser.in_waiting:
        try:
            line = ser.readline().decode('utf-8').strip()
            
            if line.startswith('ECG:'):
                value = int(line.split(':')[1])
                ecg_data[-1] = value
                
            elif line.startswith('FSR:'):
                value = int(line.split(':')[1])
                fsr_data[-1] = value
                
            elif line.startswith('RATE:'):
                latest_compression_rate = line.split(':')[1]
                
            elif line.startswith('PRESSURE:'):
                latest_pressure_class = line.split(':')[1]
                
            elif line == "!ECG_LEADS_OFF!":
                lead_off_status = True
                
        except Exception as e:
            print(f"Error processing data: {e}")
    
    # Update the plot data
    line_ecg.set_data(time_array, ecg_data)
    line_fsr.set_data(time_array, fsr_data)
    
    return line_ecg, line_fsr, compression_rate_text, pressure_text, lead_off_text

# Create animation
ani = animation.FuncAnimation(fig, update_plot, init_func=init, blit=True, interval=20)

# Add instructions
plt.figtext(0.5, 0.01, 'Press Ctrl+C in the terminal to exit', ha='center')

# Show the plot
plt.tight_layout(rect=[0, 0.03, 1, 0.95])  # Adjust layout to make room for title
try:
    plt.show()
except KeyboardInterrupt:
    print("Exiting...")
finally:
    # Close the serial port when done
    if ser.is_open:
        ser.close()
        print("Serial port closed") 