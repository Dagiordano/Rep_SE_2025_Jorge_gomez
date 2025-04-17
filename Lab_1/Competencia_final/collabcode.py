import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile
from scipy import signal
import pandas as pd
import os

# Parameters (must match ESP32 code)
NFFT = 1024
SAMPLE_RATE = 16000  # 16kHz

def normalize_spectrogram(spectrogram):
    """Normalize spectrogram to 0-1 range"""
    return (spectrogram - np.min(spectrogram)) / (np.max(spectrogram) - np.min(spectrogram))

def read_esp32_spectrogram(file_path):
    """Read the spectrogram data from ESP32 output file"""
    try:
        with open(file_path, 'r') as f:
            data = [float(line.strip()) for line in f if line.strip()]
        return np.array(data)
    except Exception as e:
        print(f"Error reading ESP32 spectrogram: {e}")
        return None

def generate_test_signal(duration=1.0):
    """Generate a test signal with multiple frequencies"""
    t = np.linspace(0, duration, int(SAMPLE_RATE * duration))
    # Generate a signal with multiple frequencies
    signal = (np.sin(2 * np.pi * 440 * t) +  # 440 Hz
             0.5 * np.sin(2 * np.pi * 880 * t) +  # 880 Hz
             0.25 * np.sin(2 * np.pi * 1320 * t))  # 1320 Hz
    return signal[:NFFT]  # Take only NFFT samples

def generate_python_spectrogram(audio_data=None):
    """Generate spectrogram using the same method as ESP32"""
    if audio_data is None:
        print("Generating test signal...")
        audio_data = generate_test_signal()
    
    # Create Hann window
    window = np.hanning(NFFT)
    
    # Normalize window
    window = window / np.sqrt(np.sum(window**2))
    
    # Apply window
    windowed_data = audio_data * window
    
    # Compute FFT
    fft_data = np.fft.rfft(windowed_data, n=NFFT)
    
    # Scale FFT output
    scale = 1.0 / np.sqrt(NFFT)
    fft_data *= scale
    
    # Convert to magnitude spectrum
    return np.abs(fft_data)

def compare_spectrograms(esp32_data, python_data):
    """Compare spectrograms and calculate error"""
    if esp32_data is None or python_data is None:
        print("Cannot compare spectrograms: One or both are None")
        return None, None
    
    # Ensure both arrays have the same length
    min_len = min(len(esp32_data), len(python_data))
    esp32_data = esp32_data[:min_len]
    python_data = python_data[:min_len]
    
    # Add small epsilon to avoid division by zero
    epsilon = 1e-10
    python_data = np.maximum(python_data, epsilon)
    
    # Calculate relative error for each point
    errors = np.abs(esp32_data - python_data) / python_data * 100
    
    # Find maximum error
    max_error = np.max(errors)
    mean_error = np.mean(errors)
    
    # Create comparison plot
    plt.figure(figsize=(15, 10))
    
    # Plot both spectrograms
    plt.subplot(2, 1, 1)
    freqs = np.linspace(0, SAMPLE_RATE/2, len(esp32_data))
    plt.plot(freqs, esp32_data, label='ESP32')
    plt.plot(freqs, python_data, label='Python', alpha=0.7)
    plt.title('Spectrogram Comparison')
    plt.xlabel('Frequency (Hz)')
    plt.ylabel('Magnitude')
    plt.legend()
    plt.grid(True)
    
    # Plot error
    plt.subplot(2, 1, 2)
    plt.plot(freqs, errors)
    plt.title('Relative Error (%)')
    plt.xlabel('Frequency (Hz)')
    plt.ylabel('Error (%)')
    plt.grid(True)
    
    plt.tight_layout()
    plt.show()
    
    # Print statistics
    print(f"Maximum error: {max_error:.2f}%")
    print(f"Mean error: {mean_error:.2f}%")
    
    # Check if all errors are below 10%
    if max_error <= 10:
        print("✅ All errors are within 10% tolerance")
    else:
        print("❌ Some errors exceed 10% tolerance")
    
    return max_error, mean_error

def main():
    # Read ESP32 spectrogram
    esp32_data = read_esp32_spectrogram('spectrogram.txt')
    if esp32_data is None:
        return
    
    print(f"ESP32 spectrogram length: {len(esp32_data)}")
    print(f"ESP32 data range: [{np.min(esp32_data)}, {np.max(esp32_data)}]")
    
    # Generate Python spectrogram
    python_data = generate_python_spectrogram()
    print(f"Python spectrogram length: {len(python_data)}")
    print(f"Python data range: [{np.min(python_data)}, {np.max(python_data)}]")
    
    # Compare and plot
    max_error, mean_error = compare_spectrograms(esp32_data, python_data)
    
    if max_error is not None and mean_error is not None:
        results = pd.DataFrame({
            'Frequency': np.linspace(0, SAMPLE_RATE/2, len(esp32_data)),
            'ESP32_Magnitude': esp32_data,
            'Python_Magnitude': python_data,
            'Error_Percent': np.abs(esp32_data - python_data) / np.maximum(python_data, 1e-10) * 100
        })
        results.to_csv('spectrogram_comparison.csv', index=False)
        print("Comparison results saved to spectrogram_comparison.csv")

if __name__ == "__main__":
    main()