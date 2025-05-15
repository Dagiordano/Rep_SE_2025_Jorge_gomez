from machine import ADC, Pin
import time

# Configure ADC
adc = ADC(Pin(36))  # GPIO36
adc.atten(ADC.ATTN_11DB)  # Full range: 3.3v
adc.width(ADC.WIDTH_12BIT)  # 12-bit resolution

# Number of samples for averaging
NUM_SAMPLES = 64

def read_fsr():
    # Take multiple samples and average them
    total = 0
    for _ in range(NUM_SAMPLES):
        total += adc.read()
    average = total / NUM_SAMPLES
    
    # Convert to voltage (ESP32 ADC is 12-bit, so 4095 is max)
    voltage = (average / 4095) * 3.3
    
    return average, voltage

# Main loop
while True:
    raw_value, voltage = read_fsr()
    print(f"Raw ADC: {raw_value:.0f}, Voltage: {voltage:.2f}V")
    time.sleep(0.1)  # Wait 100ms between readings 