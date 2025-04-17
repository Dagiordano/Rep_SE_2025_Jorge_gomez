'''


In this code we will analyse the data gathered from the several 
runs on the esp32 microcontroller.


'''
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
from scipy import stats


# Create the data dictionary
data = {
    'X value': [1000, 10000, 25000, 35000, 50000, 70000],
    'Runs': [5, 5, 5, 5, 5, 5],
    'Average time per run (us)': [166.20, 1755.40, 4377.60, 6132.00, 8760.20, 12263.20],
    'Average time per run (ms)': [0.16620, 1.75540, 4.37760, 6.13200, 8.76020, 12.26320],
    'Average cycles per run': [26615.80, 280735.59, 700246.38, 980988.00, 1401477.62, 1961977.25],
    'Average time per operation (us)': [0.033, 0.035, 0.035, 0.035, 0.035, 0.035],
    'Average cycles per operation': [5.323, 5.615, 5.602, 5.606, 5.606, 5.606],
    'Cycles Addition': [19.14, 21.01, 21.05, 21.03, 21.02, 21.02],
    'Cycles Addition with constant': [13.13, 16.03, 16.01, 16.01, 16.01, 16.01],
    'Cycles Modulo': [21.14, 24.04, 24.01, 24.02, 24.02, 24.02],
    'Cycles Multiplication': [19.14, 21.01, 21.04, 21.03, 21.02, 21.02],
    'Cycles Division': [20.01, 23.00, 23.02, 23.01, 23.02, 23.02]
}

# Create DataFrame
df = pd.DataFrame(data)

# Set X value as index for better plotting
df.set_index('X value', inplace=True)

# Calculate slope and intercept for time vs iterations
slope, intercept, r_value, p_value, std_err = stats.linregress(df.index, df['Average time per run (ms)'])

print(f"\nAnálisis de la relación tiempo vs iteraciones:")
print(f"Pendiente (slope): {slope:.6f} ms/iteración")
print(f"Intercepto: {intercept:.6f} ms")
print(f"Coeficiente de correlación (R²): {r_value**2:.6f}")
print(f"Error estándar: {std_err:.6f}")

# Save to CSV
df.to_csv('performance_results.csv')

# Example plots
plt.style.use('seaborn-v0_8')

# 1. Plot average time per run with regression line
plt.figure(figsize=(12, 6))
plt.plot(df.index, df['Average time per run (ms)'], 'o-', label='Datos')
plt.plot(df.index, slope * df.index + intercept, 'r--', label=f'Regresión lineal (y = {slope:.6f}x + {intercept:.6f})')
plt.title('Average Time per Run vs Number of Iterations')
plt.xlabel('Number of Iterations')
plt.ylabel('Time (ms)')
plt.grid(True)
plt.legend()

# Add y-value labels
for x, y in zip(df.index, df['Average time per run (ms)']):
    plt.annotate(f'{y:.4f}', (x, y), textcoords="offset points", xytext=(0,10), ha='center')

plt.savefig('time_per_run.png')
plt.close()

# 1. Plot CPI evolution vs iterations
plt.figure(figsize=(14, 8))
operations = ['Cycles Addition', 'Cycles Addition with constant', 
             'Cycles Modulo', 'Cycles Multiplication', 'Cycles Division']

# Plot CPI for each operation
for op in operations:
    plt.plot(df.index, df[op], 'o-', label=op.replace('Cycles ', ''))
    # Add y-value labels
    for x, y in zip(df.index, df[op]):
        plt.annotate(f'{y:.2f}', (x, y), textcoords="offset points", xytext=(0,10), ha='center')

plt.title('Evolución del CPI vs Número de Iteraciones')
plt.xlabel('Número de Iteraciones')
plt.ylabel('Ciclos por Instrucción (CPI)')
plt.legend()
plt.grid(True)
plt.savefig('evolucion_cpi.png')
plt.close()

# 2. Plot total cycles per operation (bar chart)
plt.figure(figsize=(12, 8))

# Calculate total cycles for the largest iteration count (70000)
x_value = 70000
total_cycles = {
    'Addition': df.loc[x_value, 'Cycles Addition'] * x_value,
    'Addition with constant': df.loc[x_value, 'Cycles Addition with constant'] * x_value,
    'Modulo': df.loc[x_value, 'Cycles Modulo'] * x_value,
    'Multiplication': df.loc[x_value, 'Cycles Multiplication'] * x_value,
    'Division': df.loc[x_value, 'Cycles Division'] * x_value
}

# Create bar chart
plt.bar(total_cycles.keys(), total_cycles.values())
plt.title('Total de Ciclos por Operación (X=70000)')
plt.xlabel('Operación')
plt.ylabel('Total de Ciclos de Reloj')
plt.xticks(rotation=45)
plt.grid(True, axis='y')

# Add value labels on top of bars
for i, v in enumerate(total_cycles.values()):
    plt.text(i, v, f'{int(v):,}', ha='center', va='bottom')

plt.tight_layout()
plt.savefig('profiling_ciclos.png')
plt.close()

# 3. Heatmap of cycle counts
plt.figure(figsize=(12, 8))
cycle_metrics = ['Cycles Addition', 'Cycles Addition with constant', 
                'Cycles Modulo', 'Cycles Multiplication', 'Cycles Division']
sns.heatmap(df[cycle_metrics], annot=True, cmap='YlOrRd', fmt='.2f')
plt.title('Heatmap of Operation Cycle Counts')
plt.savefig('cycle_heatmap.png')
plt.close()

print("Data saved to 'performance_results.csv'")
print("Plots saved as 'time_per_run.png', 'evolucion_cpi.png', 'profiling_ciclos.png', and 'cycle_heatmap.png'")


