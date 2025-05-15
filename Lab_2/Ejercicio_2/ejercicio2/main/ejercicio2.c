#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

// ADC Configuration
#define ECG_ADC_CHANNEL     ADC1_CHANNEL_0  // GPIO36
#define PRESSURE_ADC_CHANNEL ADC1_CHANNEL_3  // GPIO39
#define ADC_WIDTH           ADC_WIDTH_BIT_12
#define ADC_ATTEN          ADC_ATTEN_DB_11
#define ADC_SAMPLES        64

// LED Configuration
#define LED_GPIO           2

// Compression Detection Parameters
#define COMPRESSION_THRESHOLD  2000
#define MIN_COMPRESSION_RATE   100
#define MAX_COMPRESSION_RATE   120
#define COMPRESSION_HISTORY    10

// Timing
#define SAMPLE_INTERVAL_MS    10  // 100Hz sampling rate

// Global variables
static uint32_t compression_times[COMPRESSION_HISTORY];
static uint8_t compression_index = 0;
static uint32_t last_compression_time = 0;
static esp_adc_cal_characteristics_t adc_chars;

// Function declarations
void configure_adc(void);
void configure_led(void);
uint32_t read_adc(adc1_channel_t channel);
float calculate_compression_rate(void);
void detect_compression(uint32_t ecg_value);

void app_main(void)
{
    // Initialize ADC
    configure_adc();
    
    // Initialize LED
    configure_led();
    
    // Characterize ADC
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 1100, &adc_chars);
    
    while (1) {
        // Read sensors
        uint32_t ecg_raw = read_adc(ECG_ADC_CHANNEL);
        uint32_t pressure_raw = read_adc(PRESSURE_ADC_CHANNEL);
        
        // Convert to voltage
        float ecg_voltage = esp_adc_cal_raw_to_voltage(ecg_raw, &adc_chars) / 1000.0f;
        float pressure_voltage = esp_adc_cal_raw_to_voltage(pressure_raw, &adc_chars) / 1000.0f;
        
        // Detect compression and update LED
        detect_compression(ecg_raw);
        
        // Print data in format for Python script
        printf("ECG:%.3f,PRESSURE:%.3f\n", ecg_voltage, pressure_voltage);
        
        // Wait for next sample
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
    }
}

void configure_adc(void)
{
    // Configure ADC1
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ECG_ADC_CHANNEL, ADC_ATTEN);
    adc1_config_channel_atten(PRESSURE_ADC_CHANNEL, ADC_ATTEN);
}

void configure_led(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

uint32_t read_adc(adc1_channel_t channel)
{
    uint32_t total = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        total += adc1_get_raw(channel);
    }
    return total / ADC_SAMPLES;
}

void detect_compression(uint32_t ecg_value)
{
    int64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    
    if (ecg_value > COMPRESSION_THRESHOLD) {
        // Check if enough time has passed since last compression (debouncing)
        if (current_time - last_compression_time > 200) { // 200ms minimum between compressions
            compression_times[compression_index] = (uint32_t)current_time;
            compression_index = (compression_index + 1) % COMPRESSION_HISTORY;
            last_compression_time = (uint32_t)current_time;
            
            // Calculate rate and control LED
            float rate = calculate_compression_rate();
            if (rate < MIN_COMPRESSION_RATE || rate > MAX_COMPRESSION_RATE) {
                gpio_set_level(LED_GPIO, 1);
            } else {
                gpio_set_level(LED_GPIO, 0);
            }
        }
    }
}

float calculate_compression_rate(void)
{
    if (compression_index == 0) {
        return 0;
    }
    
    uint32_t total_time = 0;
    int count = 0;
    
    for (int i = 1; i < compression_index; i++) {
        uint32_t time_diff = compression_times[i] - compression_times[i-1];
        if (time_diff > 0) {
            total_time += time_diff;
            count++;
        }
    }
    
    if (count == 0) {
        return 0;
    }
    
    float avg_time = (float)total_time / count;
    return 60000.0f / avg_time; // Convert to compressions per minute
} 