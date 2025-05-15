#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/uart.h"
#include "esp32/rom/uart.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"

// UART Configuration
#define UART_NUM CONFIG_ESP_CONSOLE_UART_NUM  // Use the console UART
#define BUF_SIZE 1024

// Camera pins for ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define MAX_IMAGES 5  // Reduced from 20
#define IMAGE_WIDTH 160  // QQVGA width
#define IMAGE_HEIGHT 120 // QQVGA height

// Command definitions
#define CMD_GET_IMAGE 0x01
#define CMD_GET_HISTOGRAM 0x02
#define CMD_GET_SOBEL 0x03
#define CMD_GET_PERFORMANCE 0x04

// Protocol synchronization
#define SYNC_HEADER 0xAA55
#define SYNC_FOOTER 0x55AA

// Structure to hold image data and metadata
typedef struct {
    uint8_t *data;
    size_t size;
    uint64_t timestamp;
    uint8_t *histogram;
    uint8_t *sobel;
} image_buffer_t;

// Circular buffer for images
static image_buffer_t image_buffer[MAX_IMAGES];
static int current_image_index = 0;
static int total_images = 0;

// Performance measurement variables
static uint64_t capture_time = 0;
static uint64_t histogram_time = 0;
static uint64_t sobel_time = 0;
static uint64_t save_time = 0;
static uint32_t frames_processed = 0;
static uint64_t start_time = 0;

// Function declarations
static esp_err_t init_camera(void);
static void process_image(camera_fb_t *fb);
static void apply_histogram(uint8_t *input, uint8_t *output, size_t size);
static void apply_sobel(uint8_t *input, uint8_t *output, int width, int height);
static void save_image(image_buffer_t *img);
static void measure_performance(void);
static void handle_serial_commands(void *pvParameters);

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    esp_err_t uart_ret = uart_driver_install(UART_NUM, BUF_SIZE, BUF_SIZE, 0, NULL, 0);
    if (uart_ret != ESP_OK) {
        return;
    }

    // Initialize camera
    if (init_camera() != ESP_OK) {
        return;
    }

    // Initialize image buffer
    memset(image_buffer, 0, sizeof(image_buffer));

    // Start performance measurement
    start_time = esp_timer_get_time();

    // Create task for handling serial commands
    xTaskCreate(handle_serial_commands, "serial_cmd", 4096, NULL, 5, NULL);

    while (1) {
        // Capture image
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            continue;
        }

        // Process image
        process_image(fb);

        // Return the frame buffer back to the driver for reuse
        esp_camera_fb_return(fb);

        // Small delay to prevent overwhelming the system
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static esp_err_t init_camera(void)
{
    camera_config_t config = {
        .pin_pwdn = PWDN_GPIO_NUM,
        .pin_reset = RESET_GPIO_NUM,
        .pin_xclk = XCLK_GPIO_NUM,
        .pin_sccb_sda = SIOD_GPIO_NUM,
        .pin_sccb_scl = SIOC_GPIO_NUM,
        .pin_d7 = Y9_GPIO_NUM,
        .pin_d6 = Y8_GPIO_NUM,
        .pin_d5 = Y7_GPIO_NUM,
        .pin_d4 = Y6_GPIO_NUM,
        .pin_d3 = Y5_GPIO_NUM,
        .pin_d2 = Y4_GPIO_NUM,
        .pin_d1 = Y3_GPIO_NUM,
        .pin_d0 = Y2_GPIO_NUM,
        .pin_vsync = VSYNC_GPIO_NUM,
        .pin_href = HREF_GPIO_NUM,
        .pin_pclk = PCLK_GPIO_NUM,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_GRAYSCALE,
        .frame_size = FRAMESIZE_QQVGA,  // Using QQVGA resolution
        .jpeg_quality = 12,
        .fb_count = 1
    };

    return esp_camera_init(&config);
}

static void process_image(camera_fb_t *fb)
{
    uint64_t start = esp_timer_get_time();
    
    // Allocate memory for processed image
    image_buffer_t *current = &image_buffer[current_image_index];
    
    // Free previous image data if it exists
    if (current->data) {
        free(current->data);
        free(current->histogram);
        free(current->sobel);
    }

    // Allocate new memory
    current->data = malloc(fb->len);
    current->histogram = malloc(fb->len);
    current->sobel = malloc(fb->len);
    
    if (!current->data || !current->histogram || !current->sobel) {
        return;
    }

    // Copy image data
    memcpy(current->data, fb->buf, fb->len);
    current->size = fb->len;
    current->timestamp = esp_timer_get_time();

    // Apply histogram equalization
    uint64_t hist_start = esp_timer_get_time();
    apply_histogram(current->data, current->histogram, fb->len);
    histogram_time = esp_timer_get_time() - hist_start;

    // Apply Sobel filter
    uint64_t sobel_start = esp_timer_get_time();
    apply_sobel(current->data, current->sobel, fb->width, fb->height);
    sobel_time = esp_timer_get_time() - sobel_start;

    // Save the processed image
    uint64_t save_start = esp_timer_get_time();
    save_image(current);
    save_time = esp_timer_get_time() - save_start;

    // Update circular buffer index
    current_image_index = (current_image_index + 1) % MAX_IMAGES;
    if (total_images < MAX_IMAGES) {
        total_images++;
    }

    // Update performance metrics
    frames_processed++;
    capture_time = esp_timer_get_time() - start;
    measure_performance();
}

static void apply_histogram(uint8_t *input, uint8_t *output, size_t size)
{
    // Calculate histogram
    int histogram[256] = {0};
    for (size_t i = 0; i < size; i++) {
        histogram[input[i]]++;
    }

    // Calculate cumulative histogram
    int cumulative[256] = {0};
    cumulative[0] = histogram[0];
    for (int i = 1; i < 256; i++) {
        cumulative[i] = cumulative[i-1] + histogram[i];
    }

    // Normalize and apply histogram equalization
    float scale = 255.0f / size;
    for (size_t i = 0; i < size; i++) {
        output[i] = (uint8_t)(cumulative[input[i]] * scale);
    }
}

static void apply_sobel(uint8_t *input, uint8_t *output, int width, int height)
{
    // Sobel kernels
    const int sobel_x[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    
    const int sobel_y[3][3] = {
        {-1, -2, -1},
        {0, 0, 0},
        {1, 2, 1}
    };

    // Apply Sobel filter
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int gx = 0;
            int gy = 0;
            
            // Apply convolution
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int pixel = input[(y + ky) * width + (x + kx)];
                    gx += pixel * sobel_x[ky + 1][kx + 1];
                    gy += pixel * sobel_y[ky + 1][kx + 1];
                }
            }
            
            // Calculate gradient magnitude
            int magnitude = (int)sqrtf(gx * gx + gy * gy);
            output[y * width + x] = (uint8_t)(magnitude > 255 ? 255 : magnitude);
        }
    }
}

static void save_image(image_buffer_t *img)
{
    uint64_t start = esp_timer_get_time();
    
    // Here you would implement the actual saving mechanism
    // For now, we'll just simulate the save operation
    vTaskDelay(pdMS_TO_TICKS(10));
    
    save_time = esp_timer_get_time() - start;
}

static void measure_performance(void)
{
    static uint64_t last_print_time = 0;
    uint64_t current_time = esp_timer_get_time();
    
    // Print performance metrics every second
    if (current_time - last_print_time >= 1000000) {  // 1 second in microseconds
        // Reset counters
        frames_processed = 0;
        last_print_time = current_time;
    }
}

static void handle_serial_commands(void *pvParameters)
{
    uint8_t cmd;
    uint8_t image_index;
    
    while (1) {
        if (uart_read_bytes(UART_NUM, &cmd, 1, pdMS_TO_TICKS(100)) > 0) {
            switch (cmd) {
                case CMD_GET_IMAGE:
                    if (uart_read_bytes(UART_NUM, &image_index, 1, pdMS_TO_TICKS(100)) > 0) {
                        if (image_index < total_images) {
                            image_buffer_t *img = &image_buffer[image_index];
                            if (img->data) {
                                // Send sync header
                                uint16_t header = SYNC_HEADER;
                                uart_write_bytes(UART_NUM, (const char*)&header, sizeof(header));
                                // Send image size
                                uart_write_bytes(UART_NUM, (const char*)&img->size, sizeof(size_t));
                                // Send image data
                                uart_write_bytes(UART_NUM, (const char*)img->data, img->size);
                                // Send sync footer
                                uint16_t footer = SYNC_FOOTER;
                                uart_write_bytes(UART_NUM, (const char*)&footer, sizeof(footer));
                            }
                        }
                    }
                    break;
                    
                case CMD_GET_HISTOGRAM:
                    if (uart_read_bytes(UART_NUM, &image_index, 1, pdMS_TO_TICKS(100)) > 0) {
                        if (image_index < total_images) {
                            image_buffer_t *img = &image_buffer[image_index];
                            if (img->histogram) {
                                uint16_t header = SYNC_HEADER;
                                uart_write_bytes(UART_NUM, (const char*)&header, sizeof(header));
                                uart_write_bytes(UART_NUM, (const char*)&img->size, sizeof(size_t));
                                uart_write_bytes(UART_NUM, (const char*)img->histogram, img->size);
                                uint16_t footer = SYNC_FOOTER;
                                uart_write_bytes(UART_NUM, (const char*)&footer, sizeof(footer));
                            }
                        }
                    }
                    break;
                    
                case CMD_GET_SOBEL:
                    if (uart_read_bytes(UART_NUM, &image_index, 1, pdMS_TO_TICKS(100)) > 0) {
                        if (image_index < total_images) {
                            image_buffer_t *img = &image_buffer[image_index];
                            if (img->sobel) {
                                uint16_t header = SYNC_HEADER;
                                uart_write_bytes(UART_NUM, (const char*)&header, sizeof(header));
                                uart_write_bytes(UART_NUM, (const char*)&img->size, sizeof(size_t));
                                uart_write_bytes(UART_NUM, (const char*)img->sobel, img->size);
                                uint16_t footer = SYNC_FOOTER;
                                uart_write_bytes(UART_NUM, (const char*)&footer, sizeof(footer));
                            }
                        }
                    }
                    break;
                    
                case CMD_GET_PERFORMANCE:
                    {
                        uint16_t header = SYNC_HEADER;
                        uart_write_bytes(UART_NUM, (const char*)&header, sizeof(header));
                        float fps = (float)frames_processed * 1000000 / (esp_timer_get_time() - start_time);
                        uart_write_bytes(UART_NUM, (const char*)&fps, sizeof(float));
                        uart_write_bytes(UART_NUM, (const char*)&capture_time, sizeof(uint64_t));
                        uart_write_bytes(UART_NUM, (const char*)&histogram_time, sizeof(uint64_t));
                        uart_write_bytes(UART_NUM, (const char*)&sobel_time, sizeof(uint64_t));
                        uart_write_bytes(UART_NUM, (const char*)&save_time, sizeof(uint64_t));
                        uint16_t footer = SYNC_FOOTER;
                        uart_write_bytes(UART_NUM, (const char*)&footer, sizeof(footer));
                    }
                    break;
            }
        }
    }
}
