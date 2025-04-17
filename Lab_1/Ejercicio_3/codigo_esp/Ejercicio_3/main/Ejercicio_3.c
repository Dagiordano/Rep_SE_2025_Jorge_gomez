#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_spiffs.h"
#include "esp_log.h"

#define WIDTH 96
#define HEIGHT 96

static const char* TAG = "FileSystem";

// Buffers de imagen
uint8_t image[HEIGHT][WIDTH];
uint8_t result[HEIGHT][WIDTH];

// Kernels Sobel
int gx[3][3] = {
    {-1, 0, 1},
    {-2, 0, 2},
    {-1, 0, 1}
};

int gy[3][3] = {
    {-1, -2, -1},
    {0,  0,  0},
    {1,  2,  1}
};

void apply_sobel_filter() {
    for (int y = 1; y < HEIGHT - 1; y++) {
        for (int x = 1; x < WIDTH - 1; x++) {
            int sumX = 0;
            int sumY = 0;

            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    sumX += image[y+i][x+j] * gx[i+1][j+1];
                    sumY += image[y+i][x+j] * gy[i+1][j+1];
                }
            }

            int magnitude = abs(sumX) + abs(sumY);
            if (magnitude > 255) magnitude = 255;
            result[y][x] = magnitude;
        }
    }
}

void print_result_serial() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            printf("%3d ", result[y][x]);
        }
        printf("\n");
    }
}

esp_err_t load_image_from_file(const char *path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open image file: %s", path);
        return ESP_FAIL;
    }

    for (int y = 0; y < HEIGHT; y++) {
        size_t read = fread(image[y], 1, WIDTH, file);
        if (read != WIDTH) {
            ESP_LOGE(TAG, "Failed to read row %d", y);
            fclose(file);
            return ESP_FAIL;
        }
    }

    fclose(file);
    ESP_LOGI(TAG, "Image loaded from %s", path);
    return ESP_OK;
}

void app_main(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/storage",
        .partition_label = NULL,  // Puedes cambiarlo si usas un label
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t result = esp_vfs_spiffs_register(&conf);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format filesystem");
        return;
    }

    size_t total = 0, used = 0;
    result = esp_spiffs_info(conf.partition_label, &total, &used);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS info");
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Leer imagen 96x96 en escala de grises desde archivo binario
    if (load_image_from_file("/storage/imagen.raw") == ESP_OK) {
        apply_sobel_filter();
        print_result_serial();
    }

    // Si quieres mantener tu lectura de archivo de texto también:
    FILE* file = fopen("/storage/mypartition.txt", "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open text file");
    } else {
        ESP_LOGI(TAG, "Text file opened successfully");
        char line[100];
        while (fgets(line, sizeof(line), file) != NULL) {
            printf("Read line: %s", line);
        }
        fclose(file);
    }
}
