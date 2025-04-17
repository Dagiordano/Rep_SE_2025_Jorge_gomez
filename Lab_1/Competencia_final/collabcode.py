import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile
from scipy import signal
import pandas as pd
import os

# Parámetros (deben coincidir con el código del ESP32)
NFFT = 1024
SAMPLE_RATE = 16000  # 16kHz

def normalize_spectrogram(spectrogram):
    """Normaliza el espectrograma al rango 0-1"""
    return (spectrogram - np.min(spectrogram)) / (np.max(spectrogram) - np.min(spectrogram))

def read_esp32_spectrogram(file_path):
    """Lee los datos del espectrograma desde el archivo de salida del ESP32"""
    try:
        with open(file_path, 'r') as f:
            data = [float(line.strip()) for line in f if line.strip()]
        return np.array(data)
    except Exception as e:
        print(f"Error al leer el espectrograma del ESP32: {e}")
        return None

def generate_test_signal(duration=1.0):
    """Genera una señal de prueba con múltiples frecuencias"""
    t = np.linspace(0, duration, int(SAMPLE_RATE * duration))
    # Genera una señal con múltiples frecuencias
    signal = (np.sin(2 * np.pi * 440 * t) +  # 440 Hz
             0.5 * np.sin(2 * np.pi * 880 * t) +  # 880 Hz
             0.25 * np.sin(2 * np.pi * 1320 * t))  # 1320 Hz
    return signal[:NFFT]  # Toma solo NFFT muestras

def generate_python_spectrogram(audio_data=None):
    """Genera el espectrograma usando el mismo método que el ESP32"""
    if audio_data is None:
        print("Generando señal de prueba...")
        audio_data = generate_test_signal()
    
    # Crear ventana Hann
    window = np.hanning(NFFT)
    
    # Normalizar ventana
    window = window / np.sqrt(np.sum(window**2))
    
    # Aplicar ventana
    windowed_data = audio_data * window
    
    # Calcular FFT
    fft_data = np.fft.rfft(windowed_data, n=NFFT)
    
    # Escalar salida FFT
    scale = 1.0 / np.sqrt(NFFT)
    fft_data *= scale
    
    # Convertir a espectro de magnitud
    return np.abs(fft_data)

def compare_spectrograms(esp32_data, python_data):
    """Compara espectrogramas y calcula el error"""
    if esp32_data is None or python_data is None:
        print("No se pueden comparar los espectrogramas: Uno o ambos son None")
        return None, None
    
    # Asegurar que ambos arrays tengan la misma longitud
    min_len = min(len(esp32_data), len(python_data))
    esp32_data = esp32_data[:min_len]
    python_data = python_data[:min_len]
    
    # Agregar pequeño epsilon para evitar división por cero
    epsilon = 1e-10
    python_data = np.maximum(python_data, epsilon)
    
    # Calcular error relativo para cada punto
    errors = np.abs(esp32_data - python_data) / python_data * 100
    
    # Encontrar error máximo
    max_error = np.max(errors)
    mean_error = np.mean(errors)
    
    # Crear gráfico de comparación
    plt.figure(figsize=(15, 10))
    
    # Graficar ambos espectrogramas
    plt.subplot(2, 1, 1)
    freqs = np.linspace(0, SAMPLE_RATE/2, len(esp32_data))
    plt.plot(freqs, esp32_data, label='ESP32')
    plt.plot(freqs, python_data, label='Python', alpha=0.7)
    plt.title('Comparación de Espectrogramas')
    plt.xlabel('Frecuencia (Hz)')
    plt.ylabel('Magnitud')
    plt.legend()
    plt.grid(True)
    
    # Graficar error
    plt.subplot(2, 1, 2)
    plt.plot(freqs, errors)
    plt.title('Error Relativo (%)')
    plt.xlabel('Frecuencia (Hz)')
    plt.ylabel('Error (%)')
    plt.grid(True)
    
    plt.tight_layout()
    plt.show()
    
    # Imprimir estadísticas
    print(f"Error máximo: {max_error:.2f}%")
    print(f"Error medio: {mean_error:.2f}%")
    
    # Verificar si todos los errores están por debajo del 10%
    if max_error <= 10:
        print("✅ Todos los errores están dentro de la tolerancia del 10%")
    else:
        print("❌ Algunos errores exceden la tolerancia del 10%")
    
    return max_error, mean_error

def main():
    # Leer espectrograma del ESP32
    esp32_data = read_esp32_spectrogram('spectrogram.txt')
    if esp32_data is None:
        return
    
    print(f"Longitud del espectrograma ESP32: {len(esp32_data)}")
    print(f"Rango de datos ESP32: [{np.min(esp32_data)}, {np.max(esp32_data)}]")
    
    # Generar espectrograma Python
    python_data = generate_python_spectrogram()
    print(f"Longitud del espectrograma Python: {len(python_data)}")
    print(f"Rango de datos Python: [{np.min(python_data)}, {np.max(python_data)}]")
    
    # Comparar y graficar
    max_error, mean_error = compare_spectrograms(esp32_data, python_data)
    
    if max_error is not None and mean_error is not None:
        results = pd.DataFrame({
            'Frequency': np.linspace(0, SAMPLE_RATE/2, len(esp32_data)),
            'ESP32_Magnitude': esp32_data,
            'Python_Magnitude': python_data,
            'Error_Percent': np.abs(esp32_data - python_data) / np.maximum(python_data, 1e-10) * 100
        })
        results.to_csv('spectrogram_comparison.csv', index=False)
        print("Resultados de la comparación guardados en spectrogram_comparison.csv")

if __name__ == "__main__":
    main()