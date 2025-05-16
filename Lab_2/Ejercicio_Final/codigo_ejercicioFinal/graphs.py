import matplotlib.pyplot as plt
import numpy as np

# Data from the ESP32-CAM tests
frequencies = [240, 160, 80]  # CPU frequencies in MHz

# FPS values for each frequency
fps_values = [3.87, 3.85, 3.57]

# Energy per frame (mWh) for each frequency
energy_values = [0.026582, 0.023833, 0.022555]

# Performance measurement times (ms) for 240 MHz
operation_times = {
    'Capture': 0.19,
    'Histogram': 2.73,
    'Sobel': 5.26,
    'Save': 0.30,
    'Other': 240.35  # Total frame time (248.83) minus the sum of measured operations
}

# Power consumption (mW) for each frequency
power_values = [370.00, 330.00, 290.00]

# Performance per watt for each frequency
fps_per_watt = [10.45, 11.66, 12.32]

# 1. Bar chart of operation times
plt.figure(figsize=(10, 6))
operations = list(operation_times.keys())
times = list(operation_times.values())

# Create colored bars
colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd']
plt.bar(operations, times, color=colors)

plt.title('Tiempo de Procesamiento por Operación (240 MHz)', fontsize=14)
plt.ylabel('Tiempo (ms)', fontsize=12)
plt.xticks(fontsize=10, rotation=0)
plt.grid(axis='y', linestyle='--', alpha=0.7)

# Add values on top of the bars
for i, time in enumerate(times):
    if time < 10:
        plt.text(i, time + 1, f'{time} ms', ha='center', fontsize=10)
    else:
        plt.text(i, time - 10, f'{time} ms', ha='center', fontsize=10, color='white')

plt.tight_layout()
plt.savefig('1_operation_times.png', dpi=300)
plt.close()

# 2. FPS vs Frequency
plt.figure(figsize=(10, 6))
plt.plot(frequencies, fps_values, 'o-', linewidth=2, markersize=8, color='#2ca02c')
plt.title('FPS vs Frecuencia del CPU', fontsize=14)
plt.xlabel('Frecuencia del CPU (MHz)', fontsize=12)
plt.ylabel('Frames Por Segundo (FPS)', fontsize=12)
plt.grid(linestyle='--', alpha=0.7)

# Add values on the plot
for i, (freq, fps) in enumerate(zip(frequencies, fps_values)):
    plt.text(freq, fps + 0.05, f'{fps} FPS', ha='center', fontsize=10)

# Set y-axis limits to better visualize the differences
plt.ylim(3.5, 4.0)

plt.tight_layout()
plt.savefig('2_fps_vs_frequency.png', dpi=300)
plt.close()

# 3. Energy per Frame vs Frequency
plt.figure(figsize=(10, 6))
plt.plot(frequencies, energy_values, 'o-', linewidth=2, markersize=8, color='#d62728')
plt.title('Energía por Frame vs Frecuencia del CPU', fontsize=14)
plt.xlabel('Frecuencia del CPU (MHz)', fontsize=12)
plt.ylabel('Energía por Frame (mWh)', fontsize=12)
plt.grid(linestyle='--', alpha=0.7)

# Add values on the plot
for i, (freq, energy) in enumerate(zip(frequencies, energy_values)):
    plt.text(freq, energy + 0.0005, f'{energy:.6f} mWh', ha='center', fontsize=10)

plt.tight_layout()
plt.savefig('3_energy_vs_frequency.png', dpi=300)
plt.close()

# 4. Performance per Watt vs Frequency
plt.figure(figsize=(10, 6))
plt.plot(frequencies, fps_per_watt, 'o-', linewidth=2, markersize=8, color='#9467bd')
plt.title('Rendimiento por Watt vs Frecuencia del CPU', fontsize=14)
plt.xlabel('Frecuencia del CPU (MHz)', fontsize=12)
plt.ylabel('FPS/W', fontsize=12)
plt.grid(linestyle='--', alpha=0.7)

# Add values and highlight optimal configuration
for i, (freq, fpw) in enumerate(zip(frequencies, fps_per_watt)):
    label = f'{fpw} FPS/W'
    if i == 2:  # 80 MHz is optimal
        plt.text(freq, fpw + 0.2, f'{label} (ÓPTIMO)', ha='center', fontsize=10, fontweight='bold')
    else:
        plt.text(freq, fpw + 0.2, label, ha='center', fontsize=10)

plt.tight_layout()
plt.savefig('4_performance_per_watt.png', dpi=300)
plt.close()
