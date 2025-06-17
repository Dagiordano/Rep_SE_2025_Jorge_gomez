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

#ifndef TENSORFLOW_LITE_MICRO_EXAMPLES_PERSON_DETECTION_MODEL_SETTINGS_H_
#define TENSORFLOW_LITE_MICRO_EXAMPLES_PERSON_DETECTION_MODEL_SETTINGS_H_

// Keeping these as constant expressions allow us to allocate fixed-sized arrays
// on the stack for our working memory.

// All of these values are derived from the values used during model training,
// if you change your model you'll need to update these constants.
// Updated for MNIST: typically 28x28, but keeping 96x96 for camera compatibility
// The camera image will be resized/processed to fit your MNIST model input
constexpr int kNumCols = 28;  // MNIST standard size
constexpr int kNumRows = 28;  // MNIST standard size
constexpr int kNumChannels = 1;  // Grayscale

constexpr int kMaxImageSize = kNumCols * kNumRows * kNumChannels;

// MNIST has 10 classes (digits 0-9)
constexpr int kCategoryCount = 10;
constexpr int kDigitClasses = 10;

extern const char* kCategoryLabels[kCategoryCount];

#endif  // TENSORFLOW_LITE_MICRO_EXAMPLES_PERSON_DETECTION_MODEL_SETTINGS_H_
