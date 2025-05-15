from machine import ADC, Pin
import time
import array

# Configure ADCs
ecg_adc = ADC(Pin(36))  # GPIO36 for ECG
pressure_adc = ADC(Pin(39))  # GPIO39 for pressure sensor

# Configure LED
led = Pin(2, Pin.OUT)  # Built-in LED on GPIO2

# ADC Configuration
def configure_adc(adc, attenuation=ADC.ATTN_11DB):
    adc.atten(attenuation)  # Full range: 3.3v
    adc.width(ADC.WIDTH_12BIT)  # 12-bit resolution

# Initialize ADCs
configure_adc(ecg_adc)
configure_adc(pressure_adc)

# Constants
NUM_SAMPLES = 64
COMPRESSION_THRESHOLD = 2000  # Adjust based on your ECG sensor
MIN_COMPRESSION_RATE = 100
MAX_COMPRESSION_RATE = 120
COMPRESSION_WINDOW = 60  # seconds to calculate compression rate

# Variables for compression detection
compression_times = array.array('I', [0] * 10)  # Store last 10 compression timestamps
compression_index = 0
last_compression_time = 0

def read_sensors():
    # Read ECG
    ecg_total = 0
    for _ in range(NUM_SAMPLES):
        ecg_total += ecg_adc.read()
    ecg_average = ecg_total / NUM_SAMPLES
    ecg_voltage = (ecg_average / 4095) * 3.3
    
    # Read pressure
    pressure_total = 0
    for _ in range(NUM_SAMPLES):
        pressure_total += pressure_adc.read()
    pressure_average = pressure_total / NUM_SAMPLES
    pressure_voltage = (pressure_average / 4095) * 3.3
    
    return ecg_average, ecg_voltage, pressure_average, pressure_voltage

def detect_compression(ecg_value):
    global compression_times, compression_index, last_compression_time
    
    current_time = time.ticks_ms()
    
    # Simple threshold-based compression detection
    if ecg_value > COMPRESSION_THRESHOLD:
        # Check if enough time has passed since last compression (debouncing)
        if time.ticks_diff(current_time, last_compression_time) > 200:  # 200ms minimum between compressions
            compression_times[compression_index] = current_time
            compression_index = (compression_index + 1) % 10
            last_compression_time = current_time
            return True
    return False

def calculate_compression_rate():
    if compression_index == 0:
        return 0
    
    # Calculate average time between compressions
    total_time = 0
    count = 0
    
    for i in range(1, compression_index):
        time_diff = time.ticks_diff(compression_times[i], compression_times[i-1])
        if time_diff > 0:  # Avoid negative time differences
            total_time += time_diff
            count += 1
    
    if count == 0:
        return 0
    
    avg_time = total_time / count
    # Convert to compressions per minute
    rate = (60000 / avg_time) if avg_time > 0 else 0
    return rate

def adjust_adc_attenuation(ecg_value):
    # Adjust ADC attenuation based on signal level
    if ecg_value > 3500:  # Approaching saturation
        configure_adc(ecg_adc, ADC.ATTN_6DB)  # Reduce gain
    elif ecg_value < 1000:  # Signal too weak
        configure_adc(ecg_adc, ADC.ATTN_11DB)  # Increase gain

# Main loop
while True:
    ecg_raw, ecg_voltage, pressure_raw, pressure_voltage = read_sensors()
    
    # Detect compression and update rate
    if detect_compression(ecg_raw):
        compression_rate = calculate_compression_rate()
        
        # Control LED based on compression rate
        if compression_rate < MIN_COMPRESSION_RATE or compression_rate > MAX_COMPRESSION_RATE:
            led.value(1)  # Turn LED on if rate is outside target range
        else:
            led.value(0)  # Turn LED off if rate is within target range
    
    # Adjust ADC attenuation if needed
    adjust_adc_attenuation(ecg_raw)
    
    # Print data in a format suitable for plotting
    print(f"ECG:{ecg_voltage:.3f},PRESSURE:{pressure_voltage:.3f}")
    
    time.sleep(0.01)  # 10ms sampling rate (100Hz) 