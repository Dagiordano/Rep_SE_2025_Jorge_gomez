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

/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "detection_responder.h"
#include "model_settings.h"
#include "tensorflow/lite/micro/micro_log.h"

#include "esp_main.h"
#if DISPLAY_SUPPORT
#include "image_provider.h"
#include "bsp/esp-bsp.h"

// Camera definition is always initialized to match the trained detection model: 28x28 pix for MNIST
// That is too small for LCD displays, so we extrapolate the image to 224x224 pix
#define IMG_WD (28 * 8)
#define IMG_HT (28 * 8)

static lv_obj_t *camera_canvas = NULL;
static lv_obj_t *digit_indicator = NULL;
static lv_obj_t *label = NULL;
static lv_obj_t *confidence_label = NULL;

static void create_gui(void)
{
  bsp_display_start();
  bsp_display_backlight_on(); // Set display brightness to 100%
  bsp_display_lock(0);
  camera_canvas = lv_canvas_create(lv_scr_act());
  assert(camera_canvas);
  lv_obj_align(camera_canvas, LV_ALIGN_TOP_MID, 0, 0);

  digit_indicator = lv_led_create(lv_scr_act());
  assert(digit_indicator);
  lv_obj_align(digit_indicator, LV_ALIGN_BOTTOM_MID, -70, -40);
  lv_led_set_color(digit_indicator, lv_palette_main(LV_PALETTE_BLUE));

  label = lv_label_create(lv_scr_act());
  assert(label);
  lv_label_set_text_static(label, "Digit: -");
  lv_obj_align_to(label, digit_indicator, LV_ALIGN_OUT_RIGHT_MID, 20, 0);

  confidence_label = lv_label_create(lv_scr_act());
  assert(confidence_label);
  lv_label_set_text_static(confidence_label, "Confidence: 0%");
  lv_obj_align_to(confidence_label, label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
  
  bsp_display_unlock();
}
#endif // DISPLAY_SUPPORT

void RespondToDetection(TfLiteTensor* output) {
  // Find the digit with highest confidence
  int predicted_digit = 0;
  float max_confidence = -1e9f;
  float scale = output->params.scale;
  int zero_point = output->params.zero_point;

  for (int i = 0; i < kCategoryCount; i++) {
    float confidence;
    if (output->type == kTfLiteFloat32) {
      confidence = output->data.f[i];
    } else {
      confidence = (output->data.int8[i] - zero_point) * scale;
    }
    if (confidence > max_confidence) {
      max_confidence = confidence;
      predicted_digit = i;
    }
  }

  int confidence_percentage = (max_confidence * 100.0f) + 0.5f;

#if DISPLAY_SUPPORT
    if (!camera_canvas) {
      create_gui();
    }

    uint16_t *buf = (uint16_t *) image_provider_get_display_buf();

    bsp_display_lock(0);
    
    // Update digit display
    char digit_text[32];
    char confidence_text[32];
    
    snprintf(digit_text, sizeof(digit_text), "Digit: %d", predicted_digit);
    snprintf(confidence_text, sizeof(confidence_text), "Confidence: %d%%", confidence_percentage);
    
    lv_label_set_text(label, digit_text);
    lv_label_set_text(confidence_label, confidence_text);
    
    if (confidence_percentage > 60) { // High confidence detection
      lv_led_on(digit_indicator);
      lv_led_set_color(digit_indicator, lv_palette_main(LV_PALETTE_GREEN));
    } else if (confidence_percentage > 30) { // Medium confidence
      lv_led_on(digit_indicator);
      lv_led_set_color(digit_indicator, lv_palette_main(LV_PALETTE_ORANGE));
    } else { // Low confidence
      lv_led_off(digit_indicator);
    }
    
    lv_canvas_set_buffer(camera_canvas, buf, IMG_WD, IMG_HT, LV_IMG_CF_TRUE_COLOR);
    bsp_display_unlock();
#endif // DISPLAY_SUPPORT

  MicroPrintf("Detected digit: %d with confidence: %d%%", predicted_digit, confidence_percentage);
  
  // Print all confidences for debugging
  MicroPrintf("All confidences:");
  for (int i = 0; i < kCategoryCount; i++) {
    float confidence;
    if (output->type == kTfLiteFloat32) {
      confidence = output->data.f[i];
    } else {
      confidence = (output->data.int8[i] - zero_point) * scale;
    }
    MicroPrintf("  Digit %d: %.1f%%", i, confidence * 100.0f);
  }
}
