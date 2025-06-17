/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "app_camera_esp.h"
#include "esp_camera.h"
#include "model_settings.h"
#include "image_provider.h"
#include "esp_main.h"

static const char* TAG = "app_camera";

static uint16_t *display_buf; // buffer to hold data to be sent to display

// Get the camera module ready
TfLiteStatus InitCamera() {
#if CLI_ONLY_INFERENCE
  ESP_LOGI(TAG, "CLI_ONLY_INFERENCE enabled, skipping camera init");
  return kTfLiteOk;
#endif
// if display support is present, initialise display buf
#if DISPLAY_SUPPORT
  if (display_buf == NULL) {
    // Size of display_buf:
    // Frame 28x28 from camera is extrapolated to 224x224 (28*8). RGB565 pixel format -> 2 bytes per pixel
    display_buf = (uint16_t *) heap_caps_malloc(28 * 8 * 28 * 8 * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (display_buf == NULL) {
    ESP_LOGE(TAG, "Couldn't allocate display buffer");
    return kTfLiteError;
  }
#endif // DISPLAY_SUPPORT

#if ESP_CAMERA_SUPPORTED
  int ret = app_camera_init();
  if (ret != 0) {
    MicroPrintf("Camera init failed\n");
    return kTfLiteError;
  }
  MicroPrintf("Camera Initialized\n");
#else
  ESP_LOGE(TAG, "Camera not supported for this device");
#endif
  return kTfLiteOk;
}

void *image_provider_get_display_buf()
{
  return (void *) display_buf;
}

// Get an image from the camera module
TfLiteStatus GetImage(int image_width, int image_height, int channels, int8_t* image_data) {
#if ESP_CAMERA_SUPPORTED
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    ESP_LOGE(TAG, "Camera capture failed");
    return kTfLiteError;
  }

  const int src_width = 96;
  const int src_height = 96;
  const int dst_width = 28;
  const int dst_height = 28;

  // Step 1: Use grayscale image and apply adaptive thresholding
  static uint8_t grayscale_img[96 * 96];
  static uint8_t thresholded_img[96 * 96];
  
  // First get grayscale image and flip horizontally
  static uint8_t temp_buf[96 * 96];
  for (int y = 0; y < src_height; y++) {
    for (int x = 0; x < src_width; x++) {
      // Flip horizontally by reading from right to left
      temp_buf[y * src_width + x] = ((uint8_t *)fb->buf)[y * src_width + (src_width - 1 - x)];
    }
  }
  
  // Copy flipped image to grayscale buffer
  memcpy(grayscale_img, temp_buf, src_width * src_height);

  // Debug: Print original grayscale values (now horizontally flipped)
  printf("ORIGINAL_96x96_START\n");
  for (int y = 0; y < src_height; y += 4) {
    for (int x = 0; x < src_width; x += 4) {
      printf("%3d,", grayscale_img[y * src_width + x]);
    }
    printf("\n");
  }
  printf("ORIGINAL_96x96_END\n");

  // Apply adaptive thresholding with adjusted parameters
  const int window_size = 31;  // Larger window to capture more context
  const int t = 7;           // Smaller threshold for more sensitivity

  for (int y = 0; y < src_height; y++) {
    for (int x = 0; x < src_width; x++) {
      // Calculate local mean
      int sum = 0;
      int count = 0;
      
      for (int wy = -window_size/2; wy <= window_size/2; wy++) {
        for (int wx = -window_size/2; wx <= window_size/2; wx++) {
          int ny = y + wy;
          int nx = x + wx;
          
          if (ny >= 0 && ny < src_height && nx >= 0 && nx < src_width) {
            sum += grayscale_img[ny * src_width + nx];
            count++;
          }
        }
      }
      
      int mean = sum / count;
      int pixel = grayscale_img[y * src_width + x];
      
      // If pixel is t less than local mean, set to white (255), else black (0)
      // Note: We invert here to match MNIST format (white digit on black background)
      thresholded_img[y * src_width + x] = (pixel < mean - t) ? 255 : 0;
    }
  }

  // Debug: Print thresholded values
  printf("THRESHOLDED_96x96_START\n");
  for (int y = 0; y < src_height; y += 4) {
    for (int x = 0; x < src_width; x += 4) {
      printf("%3d,", thresholded_img[y * src_width + x]);
    }
    printf("\n");
  }
  printf("THRESHOLDED_96x96_END\n");

  // Step 3: Downsample thresholded image to 28x28 using averaging
  static uint8_t downsampled_img[28 * 28];
  const int downsample_factor = 3; // 96/28 ≈ 3.4, we use 3 for simplicity
  
  for (int y = 0; y < dst_height; y++) {
    for (int x = 0; x < dst_width; x++) {
      // Average over downsample_factor x downsample_factor window
      int sum = 0;
      int count = 0;
      for (int dy = 0; dy < downsample_factor && (y * downsample_factor + dy) < src_height; dy++) {
        for (int dx = 0; dx < downsample_factor && (x * downsample_factor + dx) < src_width; dx++) {
          sum += thresholded_img[(y * downsample_factor + dy) * src_width + (x * downsample_factor + dx)];
          count++;
        }
      }
      downsampled_img[y * dst_width + x] = sum / count;
    }
  }

  // Debug: Print downsampled values
  printf("DOWNSAMPLED_28x28_START\n");
  for (int y = 0; y < dst_height; y++) {
    for (int x = 0; x < dst_width; x++) {
      printf("%3d,", downsampled_img[y * dst_width + x]);
    }
    printf("\n");
  }
  printf("DOWNSAMPLED_28x28_END\n");

  // Step 4: Quantize to int8 using proper scale and zero_point
  const float quant_scale = 0.003921568859368563f;  // 1/255
  const int zero_point = -128;
  
  for (int i = 0; i < dst_width * dst_height; i++) {
    // Convert to float, scale, then convert to int8_t
    float scaled_val = downsampled_img[i] * quant_scale;
    image_data[i] = (int8_t)(scaled_val * 255.0f + zero_point);
  }

  // Debug: Print final quantized values
  printf("QUANTIZED_28x28_START\n");
  for (int y = 0; y < dst_height; y++) {
    for (int x = 0; x < dst_width; x++) {
      printf("%4d,", image_data[y * dst_width + x]);
    }
    printf("\n");
  }
  printf("QUANTIZED_28x28_END\n");

  esp_camera_fb_return(fb);
  return kTfLiteOk;
#else
  return kTfLiteError;
#endif
}

