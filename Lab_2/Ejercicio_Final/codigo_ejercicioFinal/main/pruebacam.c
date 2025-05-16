#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp32/rom/lldesc.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "soc/rtc.h"
#include "esp_pm.h"
#include "esp_psram.h"
#include "esp_cpu.h"

static const char *TAG = "ESP32-CAM";

// Camera pin definitions for ESP32-CAM
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

// Settings
#define MAX_IMAGES      10  // Reduced from 20 to 10 to save memory
#define FRAME_SIZE      FRAMESIZE_VGA  // 640x480
#define IMAGE_FORMAT    PIXFORMAT_JPEG
#define JPEG_QUALITY    10  // 0-63, lower is higher quality

// Image buffer structure
typedef struct {
    uint8_t *buf;
    size_t len;
    bool used;
} image_buffer_t;

// Image processing buffers (stored in PSRAM)
typedef struct {
    image_buffer_t histogram;
    image_buffer_t sobel;
} image_set_t;

// Processing stats
typedef struct {
    int64_t capture_time;
    int64_t histogram_time;
    int64_t sobel_time;
    int64_t save_time;
    int64_t total_time;
} frame_stats_t;

static frame_stats_t stats = {0};
static int current_image_idx = 0;
static int total_images = 0;
static image_set_t image_sets[MAX_IMAGES];

// Forward declarations
static esp_err_t init_camera(void);
static void process_image(camera_fb_t *fb);
static void apply_histogram_equalization(uint8_t *image, size_t width, size_t height, image_buffer_t *output);
static void apply_sobel_filter(uint8_t *image, size_t width, size_t height, image_buffer_t *output);
static void free_oldest_image_set(void);
static void print_stats(void);
static void set_cpu_frequency(int freq_mhz);
static float estimate_power_consumption(int cpu_freq_mhz, float fps);

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Print heap info
    ESP_LOGI(TAG, "Initial heap - Total: %d bytes, Free: %d bytes", 
             heap_caps_get_total_size(MALLOC_CAP_8BIT),
             heap_caps_get_free_size(MALLOC_CAP_8BIT));

    // Print system info
    ESP_LOGI(TAG, "Starting ESP32-CAM application");
    ESP_LOGI(TAG, "ESP-IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "CPU frequency: %d MHz", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    ESP_LOGI(TAG, "Main task stack size: %d bytes", CONFIG_ESP_MAIN_TASK_STACK_SIZE);
    
    // Check PSRAM support in configuration
    #ifdef CONFIG_ESP32_SPIRAM_SUPPORT
        ESP_LOGI(TAG, "PSRAM is enabled in configuration");
    #else
        ESP_LOGE(TAG, "PSRAM is not enabled in configuration! Enable CONFIG_ESP32_SPIRAM_SUPPORT.");
        while(1) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    #endif
    
    // Check PSRAM availability
    size_t psram_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    
    ESP_LOGI(TAG, "PSRAM total size: %d bytes (%.2f MB)", psram_total, psram_total / 1024.0 / 1024.0);
    ESP_LOGI(TAG, "PSRAM free size: %d bytes (%.2f MB)", psram_size, psram_size / 1024.0 / 1024.0);
    
    if (psram_size == 0 || psram_total == 0) {
        ESP_LOGE(TAG, "No PSRAM detected or enabled! This application requires PSRAM.");
        ESP_LOGE(TAG, "Possible causes:");
        ESP_LOGE(TAG, "1. Your ESP32-CAM model doesn't have PSRAM");
        ESP_LOGE(TAG, "2. PSRAM is not properly enabled in menuconfig");
        ESP_LOGE(TAG, "3. PSRAM hardware initialization failed");
        ESP_LOGE(TAG, "Please verify your hardware and configuration.");
        
        ESP_LOGW(TAG, "Continuing with reduced functionality - will use regular memory instead");
    }

    // Initialize image buffers
    for (int i = 0; i < MAX_IMAGES; i++) {
        image_sets[i].histogram.buf = NULL;
        image_sets[i].histogram.len = 0;
        image_sets[i].histogram.used = false;
        
        image_sets[i].sobel.buf = NULL;
        image_sets[i].sobel.len = 0;
        image_sets[i].sobel.used = false;
    }

    // Initialize camera
    ESP_LOGI(TAG, "Initializing camera...");
    ret = init_camera();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed with error 0x%x", ret);
        return;
    }
    ESP_LOGI(TAG, "Camera initialized successfully");

    // Print heap info after camera init
    ESP_LOGI(TAG, "Heap after camera init - Total: %d bytes, Free: %d bytes", 
             heap_caps_get_total_size(MALLOC_CAP_8BIT),
             heap_caps_get_free_size(MALLOC_CAP_8BIT));

    ESP_LOGI(TAG, "Starting image capture and processing tests...");

    // Array of CPU frequencies to test (MHz)
    int cpu_frequencies[] = {240, 160, 80};
    int num_frequencies = sizeof(cpu_frequencies) / sizeof(cpu_frequencies[0]);
    
    // Results arrays
    float fps_results[sizeof(cpu_frequencies) / sizeof(cpu_frequencies[0])];
    float power_results[sizeof(cpu_frequencies) / sizeof(cpu_frequencies[0])];
    float fps_per_watt[sizeof(cpu_frequencies) / sizeof(cpu_frequencies[0])];
    
    // Test each CPU frequency
    for (int freq_idx = 0; freq_idx < num_frequencies; freq_idx++) {
        int freq = cpu_frequencies[freq_idx];
        
        // Set CPU frequency
        set_cpu_frequency(freq);
        
        // Wait for frequency change to stabilize
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(TAG, "Testing with CPU frequency: %d MHz", freq);
        
        // Reset stats
        memset(&stats, 0, sizeof(stats));
        
        // Capture and process images for this frequency
        int64_t start_time = esp_timer_get_time();
        int frames = 0;
        float fps = 0;
        
        // Process 30 frames for each frequency
        for (int i = 0; i < 30; i++) {
            int64_t frame_start = esp_timer_get_time();
            
            // Capture image
            int64_t capture_start = esp_timer_get_time();
            camera_fb_t *fb = esp_camera_fb_get();
            if (!fb) {
                ESP_LOGE(TAG, "Camera capture failed");
                continue;
            }
            stats.capture_time += esp_timer_get_time() - capture_start;
            
            // Process image
            process_image(fb);
            
            // Return frame buffer
            esp_camera_fb_return(fb);
            
            // Calculate frame time
            int64_t frame_time = esp_timer_get_time() - frame_start;
            stats.total_time += frame_time;
            
            frames++;
            
            // Small delay to prevent watchdog issues
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        // Calculate and report average FPS
        int64_t test_duration = esp_timer_get_time() - start_time;
        fps = (float)frames / (test_duration / 1000000.0f);
        fps_results[freq_idx] = fps;
        
        // Calculate average processing times
        float avg_capture_ms = stats.capture_time / (1000.0f * frames);
        float avg_hist_ms = stats.histogram_time / (1000.0f * frames);
        float avg_sobel_ms = stats.sobel_time / (1000.0f * frames);
        float avg_save_ms = stats.save_time / (1000.0f * frames);
        float avg_total_ms = stats.total_time / (1000.0f * frames);
        
        ESP_LOGI(TAG, "=== Results for %d MHz ===", freq);
        ESP_LOGI(TAG, "Avg. FPS: %.2f", fps);
        ESP_LOGI(TAG, "Avg. frame time: %.2f ms", avg_total_ms);
        ESP_LOGI(TAG, "Avg. capture time: %.2f ms (%.1f%%)", 
                avg_capture_ms, (avg_capture_ms / avg_total_ms) * 100);
        ESP_LOGI(TAG, "Avg. histogram time: %.2f ms (%.1f%%)", 
                avg_hist_ms, (avg_hist_ms / avg_total_ms) * 100);
        ESP_LOGI(TAG, "Avg. Sobel time: %.2f ms (%.1f%%)", 
                avg_sobel_ms, (avg_sobel_ms / avg_total_ms) * 100);
        ESP_LOGI(TAG, "Avg. save time: %.2f ms (%.1f%%)", 
                avg_save_ms, (avg_save_ms / avg_total_ms) * 100);
        
        // Calculate power consumption
        float power = estimate_power_consumption(freq, fps);
        power_results[freq_idx] = power;
        
        // Calculate performance per watt
        fps_per_watt[freq_idx] = fps / (power / 1000.0f); // FPS per Watt
        
        ESP_LOGI(TAG, "Performance per Watt: %.2f FPS/W", fps_per_watt[freq_idx]);
        
        // Calculate energy usage for 10 days
        float frames_in_10_days = fps * 60 * 60 * 24 * 10;
        float energy_per_frame_wh = power / (fps * 3600.0f); // Wh per frame
        float total_energy_wh = frames_in_10_days * energy_per_frame_wh;
        
        ESP_LOGI(TAG, "Total energy for 10 days: %.2f Wh (%.2f mAh at 3.7V)", 
                total_energy_wh, (total_energy_wh / 3.7f) * 1000);
                
        // Wait between frequency changes
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Find optimal frequency for FPS/W
    float max_fps_per_watt = 0;
    int optimal_freq_idx = 0;
    
    for (int i = 0; i < num_frequencies; i++) {
        if (fps_per_watt[i] > max_fps_per_watt) {
            max_fps_per_watt = fps_per_watt[i];
            optimal_freq_idx = i;
        }
    }
    
    ESP_LOGI(TAG, "======== SUMMARY ========");
    ESP_LOGI(TAG, "CPU Freq (MHz) | FPS     | Power (mW) | FPS/Watt");
    for (int i = 0; i < num_frequencies; i++) {
        ESP_LOGI(TAG, "%-14d | %-7.2f | %-10.2f | %.2f %s", 
                cpu_frequencies[i], fps_results[i], power_results[i], fps_per_watt[i],
                (i == optimal_freq_idx) ? "(OPTIMAL)" : "");
    }
    
    // Done with tests, restore default frequency
    set_cpu_frequency(240);
    
    ESP_LOGI(TAG, "All tests completed. Entering continuous capture mode...");
    
    // Reset stats for continuous operation
    memset(&stats, 0, sizeof(stats));
    
    // Start continuous capture operation
    int64_t start_time = esp_timer_get_time();
    int frames = 0;
    
    while (1) {
        int64_t frame_start = esp_timer_get_time();
        
        // Capture image
        int64_t capture_start = esp_timer_get_time();
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            continue;
        }
        stats.capture_time = esp_timer_get_time() - capture_start;
        
        // Process and save the image
        process_image(fb);
        
        // Return frame buffer
        esp_camera_fb_return(fb);
        
        stats.total_time = esp_timer_get_time() - frame_start;
        
        // Calculate and print FPS every 10 frames
        frames++;
        if (frames % 10 == 0) {
            int64_t current_time = esp_timer_get_time();
            float fps = (float)frames / ((current_time - start_time) / 1000000.0);
            ESP_LOGI(TAG, "FPS: %.2f", fps);
            print_stats();
            
            // Reset for next measurement
            start_time = current_time;
            frames = 0;
        }
        
        // Small delay to prevent watchdog issues
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static esp_err_t init_camera(void)
{
    camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
        
        .xclk_freq_hz = 20000000,  // 20MHz
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        
        .pixel_format = IMAGE_FORMAT,
        .frame_size = FRAME_SIZE,
        
        .jpeg_quality = JPEG_QUALITY,
        .fb_count = 2,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };
    
    return esp_camera_init(&camera_config);
}

static void process_image(camera_fb_t *fb)
{
    // If we have more than MAX_IMAGES, free the oldest set
    if (total_images >= MAX_IMAGES) {
        free_oldest_image_set();
    }
    
    int64_t save_start = esp_timer_get_time();
    
    // Check memory before proceeding
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    if (free_heap < 20000) {
        ESP_LOGW(TAG, "Low memory: %d bytes, skipping image processing", free_heap);
        return;
    }
    
    // Convert JPEG to grayscale
    size_t gray_width = fb->width;
    size_t gray_height = fb->height;
    size_t gray_size = gray_width * gray_height;
    
    // If the image is too large, scale it down
    if (gray_size > 100000) {
        gray_width = fb->width / 2;
        gray_height = fb->height / 2;
        gray_size = gray_width * gray_height;
        ESP_LOGI(TAG, "Scaling down image for processing: %dx%d -> %dx%d", 
                fb->width, fb->height, gray_width, gray_height);
    }
    
    // Allocate memory for grayscale image
    uint8_t *grayscale_img = heap_caps_malloc(gray_size, MALLOC_CAP_SPIRAM);
    if (!grayscale_img) {
        ESP_LOGW(TAG, "Could not allocate PSRAM for grayscale image, using regular memory");
        grayscale_img = malloc(gray_size);
        if (!grayscale_img) {
            ESP_LOGE(TAG, "Failed to allocate memory for grayscale image");
            return;
        }
    }
    
    // Convert JPEG to grayscale
    if (fb->format == PIXFORMAT_JPEG) {
        // For JPEG, we need to decode it first
        camera_fb_t *rgb_fb = esp_camera_fb_get();
        if (!rgb_fb) {
            ESP_LOGE(TAG, "Failed to get RGB frame buffer");
            free(grayscale_img);
            return;
        }
        
        // Convert RGB to grayscale
        for (size_t i = 0; i < gray_size; i++) {
            // RGB888 format: 3 bytes per pixel
            size_t rgb_idx = i * 3;
            // Convert to grayscale using standard luminance formula
            grayscale_img[i] = (uint8_t)((rgb_fb->buf[rgb_idx] * 0.299f) + 
                                        (rgb_fb->buf[rgb_idx + 1] * 0.587f) + 
                                        (rgb_fb->buf[rgb_idx + 2] * 0.114f));
        }
        
        // Return the RGB frame buffer
        esp_camera_fb_return(rgb_fb);
    } else {
        ESP_LOGE(TAG, "Unsupported pixel format");
        free(grayscale_img);
        return;
    }
    
    stats.save_time = esp_timer_get_time() - save_start;
    
    // Apply histogram equalization
    int64_t hist_start = esp_timer_get_time();
    apply_histogram_equalization(grayscale_img, gray_width, gray_height, &image_sets[current_image_idx].histogram);
    stats.histogram_time = esp_timer_get_time() - hist_start;
    
    // Check memory again before proceeding with Sobel filter
    free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    if (free_heap < 20000) {
        ESP_LOGW(TAG, "Low memory after histogram: %d bytes, skipping Sobel filter", free_heap);
        // Free the grayscale image
        free(grayscale_img);
        
        // Update indices even though we only did histogram
        current_image_idx = (current_image_idx + 1) % MAX_IMAGES;
        if (total_images < MAX_IMAGES) {
            total_images++;
        }
        
        return;
    }
    
    // Apply Sobel filter
    int64_t sobel_start = esp_timer_get_time();
    apply_sobel_filter(grayscale_img, gray_width, gray_height, &image_sets[current_image_idx].sobel);
    stats.sobel_time = esp_timer_get_time() - sobel_start;
    
    // Free temporary buffer
    free(grayscale_img);
    
    // Update indices
    current_image_idx = (current_image_idx + 1) % MAX_IMAGES;
    if (total_images < MAX_IMAGES) {
        total_images++;
    }
    
    // Log memory usage (only in continuous capture mode)
    static int capture_count = 0;
    capture_count++;
    if (capture_count % 10 == 0) {
        size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGI(TAG, "Memory: Free DRAM: %d bytes, Free PSRAM: %d bytes", 
                free_dram, free_psram);
    }
}

static void apply_histogram_equalization(uint8_t *image, size_t width, size_t height, image_buffer_t *output)
{
    size_t total_pixels = width * height;
    
    // Allocate/reallocate memory for histogram equalized image
    if (output->buf != NULL) {
        free(output->buf);
        output->buf = NULL;
    }
    
    // Try to allocate in PSRAM first, fall back to regular memory
    output->buf = heap_caps_malloc(total_pixels, MALLOC_CAP_SPIRAM);
    if (!output->buf) {
        ESP_LOGW(TAG, "Could not allocate PSRAM for histogram image, using regular memory");
        output->buf = malloc(total_pixels);
        if (!output->buf) {
            ESP_LOGE(TAG, "Failed to allocate memory for histogram image");
            return;
        }
    }
    
    output->len = total_pixels;
    output->used = true;
    
    // Copy the original grayscale image to the output buffer
    memcpy(output->buf, image, total_pixels);
    
    // Assuming grayscale image (1 byte per pixel)
    // Step 1: Calculate histogram - use stack memory for histograms
    uint32_t histogram[256] = {0};
    uint32_t cdf[256] = {0};
    uint8_t lut[256] = {0};
    
    // Count pixel occurrences
    for (size_t i = 0; i < total_pixels; i++) {
        histogram[output->buf[i]]++;
    }
    
    // Step 2: Calculate cumulative distribution function (CDF)
    cdf[0] = histogram[0];
    for (int i = 1; i < 256; i++) {
        cdf[i] = cdf[i-1] + histogram[i];
    }
    
    // Step 3: Normalize CDF to range [0, 255]
    uint32_t cdf_min = 0;
    // Find first non-zero value in CDF
    for (int i = 0; i < 256; i++) {
        if (cdf[i] > 0) {
            cdf_min = cdf[i];
            break;
        }
    }
    
    // Create lookup table for equalization
    for (int i = 0; i < 256; i++) {
        if (cdf[i] <= cdf_min) {
            lut[i] = 0;
        } else {
            // Avoid potential divide by zero
            uint32_t denom = (total_pixels - cdf_min);
            if (denom > 0) {
                float temp = ((float)(cdf[i] - cdf_min) / denom) * 255.0f;
                lut[i] = (temp > 255) ? 255 : (uint8_t)temp;
            } else {
                lut[i] = i;
            }
        }
    }
    
    // Step 4: Map original pixels through lookup table in chunks to reduce stack usage
    const size_t chunk_size = 1024; // Process 1KB at a time
    
    for (size_t offset = 0; offset < total_pixels; offset += chunk_size) {
        size_t chunk_end = offset + chunk_size;
        if (chunk_end > total_pixels) chunk_end = total_pixels;
        
        for (size_t i = offset; i < chunk_end; i++) {
            output->buf[i] = lut[output->buf[i]];
        }
        
        // Small yield to prevent watchdog from triggering on large images
        if (offset % (chunk_size * 16) == 0) {
            vTaskDelay(1);
        }
    }
}

static void apply_sobel_filter(uint8_t *image, size_t width, size_t height, image_buffer_t *output)
{
    size_t total_pixels = width * height;
    
    // Allocate/reallocate memory for Sobel filtered image
    if (output->buf != NULL) {
        free(output->buf);
        output->buf = NULL;
    }
    
    // Try to allocate in PSRAM first, fall back to regular memory
    output->buf = heap_caps_malloc(total_pixels, MALLOC_CAP_SPIRAM);
    if (!output->buf) {
        ESP_LOGW(TAG, "Could not allocate PSRAM for Sobel image, using regular memory");
        output->buf = malloc(total_pixels);
        if (!output->buf) {
            ESP_LOGE(TAG, "Failed to allocate memory for Sobel image");
            return;
        }
    }
    
    output->len = total_pixels;
    output->used = true;
    
    // Initialize output to zeros
    memset(output->buf, 0, total_pixels);
    
    // Check if we have enough memory to process the image
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    if (free_heap < 10000) {
        ESP_LOGW(TAG, "Not enough memory for Sobel processing: %d bytes available", free_heap);
        // Just fill with a pattern to indicate it was processed
        for (size_t i = 0; i < total_pixels; i++) {
            output->buf[i] = (i % 256);
        }
        return;
    }
    
    // Process the image in chunks to reduce stack usage
    const size_t chunk_height = 16; // Process 16 rows at a time
    
    for (size_t chunk_start = 1; chunk_start < height - 1; chunk_start += chunk_height) {
        size_t chunk_end = chunk_start + chunk_height;
        if (chunk_end > height - 1) chunk_end = height - 1;
        
        // Apply Sobel operators to this chunk
        for (size_t y = chunk_start; y < chunk_end; y++) {
            for (size_t x = 1; x < width - 1; x++) {
                // Pixel indexes for 3x3 kernel
                size_t p1 = (y-1) * width + (x-1);
                size_t p2 = (y-1) * width + x;
                size_t p3 = (y-1) * width + (x+1);
                size_t p4 = y * width + (x-1);
                size_t p6 = y * width + (x+1);
                size_t p7 = (y+1) * width + (x-1);
                size_t p8 = (y+1) * width + x;
                size_t p9 = (y+1) * width + (x+1);
                
                // Sobel X operator
                int sx = -image[p1] - 2*image[p4] - image[p7] + image[p3] + 2*image[p6] + image[p9];
                // Sobel Y operator
                int sy = -image[p1] - 2*image[p2] - image[p3] + image[p7] + 2*image[p8] + image[p9];
                
                // Gradient magnitude approximation (|Gx| + |Gy| for performance)
                int edge_strength = abs(sx) + abs(sy);
                
                // Clamp to 0-255
                if (edge_strength > 255) edge_strength = 255;
                
                // Store in output
                output->buf[y * width + x] = (uint8_t)edge_strength;
            }
        }
        
        // Small yield to prevent watchdog from triggering
        vTaskDelay(1);
    }
}

static void free_oldest_image_set(void)
{
    int oldest_idx = current_image_idx;
    
    // Free memory for the oldest images
    if (image_sets[oldest_idx].histogram.buf != NULL) {
        free(image_sets[oldest_idx].histogram.buf);
        image_sets[oldest_idx].histogram.buf = NULL;
        image_sets[oldest_idx].histogram.len = 0;
        image_sets[oldest_idx].histogram.used = false;
    }
    
    if (image_sets[oldest_idx].sobel.buf != NULL) {
        free(image_sets[oldest_idx].sobel.buf);
        image_sets[oldest_idx].sobel.buf = NULL;
        image_sets[oldest_idx].sobel.len = 0;
        image_sets[oldest_idx].sobel.used = false;
    }
    
    ESP_LOGI(TAG, "Freed oldest image set (index %d)", oldest_idx);
}

static void print_stats(void)
{
    float capture_ms = stats.capture_time / 1000.0;
    float hist_ms = stats.histogram_time / 1000.0;
    float sobel_ms = stats.sobel_time / 1000.0;
    float save_ms = stats.save_time / 1000.0;
    float total_ms = stats.total_time / 1000.0;
    
    ESP_LOGI(TAG, "Frame processing times:");
    ESP_LOGI(TAG, "  Capture:    %.2f ms", capture_ms);
    ESP_LOGI(TAG, "  Histogram:  %.2f ms", hist_ms);
    ESP_LOGI(TAG, "  Sobel:      %.2f ms", sobel_ms);
    ESP_LOGI(TAG, "  Save:       %.2f ms", save_ms);
    ESP_LOGI(TAG, "  Total:      %.2f ms", total_ms);
    
    // Calculate bottleneck
    float max_time = capture_ms;
    const char *bottleneck = "Capture";
    
    if (hist_ms > max_time) {
        max_time = hist_ms;
        bottleneck = "Histogram";
    }
    if (sobel_ms > max_time) {
        max_time = sobel_ms;
        bottleneck = "Sobel";
    }
    if (save_ms > max_time) {
        max_time = save_ms;
        bottleneck = "Save";
    }
    
    ESP_LOGI(TAG, "Bottleneck: %s (%.2f ms)", bottleneck, max_time);
    
    // Get CPU frequency (in MHz)
    int cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    ESP_LOGI(TAG, "CPU Frequency: %d MHz", cpu_freq_mhz);
}

/**
 * Change CPU frequency
 * Available options (MHz): 
 * - 240 (default)
 * - 160
 * - 80
 * - 40
 */
static void set_cpu_frequency(int freq_mhz) {
    esp_pm_config_t pm_config = {
        .max_freq_mhz = freq_mhz,        // Set max frequency 
        .min_freq_mhz = freq_mhz,        // Set min frequency to the same value to lock frequency
        .light_sleep_enable = false       // No light sleep for this experiment
    };
    
    ESP_LOGI(TAG, "Setting CPU frequency to %d MHz", freq_mhz);
    esp_err_t ret = esp_pm_configure(&pm_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set CPU frequency: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "CPU frequency set successfully");
    }
}

/**
 * Measure power consumption (simulation since we can't directly measure power)
 * This is a placeholder that would be replaced with actual power measurements in a real setup
 */
static float estimate_power_consumption(int cpu_freq_mhz, float fps) {
    // These values are approximations and would need to be replaced with real measurements
    // Baseline power in mW
    float baseline_power = 100.0f;
    
    // Additional power per MHz in mW/MHz (estimated)
    float power_per_mhz = 0.5f;
    
    // Additional power for camera in mW
    float camera_power = 120.0f;
    
    // Additional power for PSRAM usage in mW (estimated)
    float psram_power = 30.0f;
    
    // Calculate approximate power
    float cpu_power = baseline_power + (power_per_mhz * cpu_freq_mhz);
    float total_power = cpu_power + camera_power + psram_power;
    
    // Power per frame in mWh per frame
    float power_per_frame = total_power / (fps * 3600.0f);
    
    ESP_LOGI(TAG, "Estimated power consumption: %.2f mW, %.6f mWh per frame", 
             total_power, power_per_frame);
             
    return total_power;
}
