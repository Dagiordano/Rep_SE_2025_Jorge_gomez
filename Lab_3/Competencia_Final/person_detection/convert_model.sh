#!/bin/bash

# Script to convert TensorFlow Lite model to C++ array format for ESP32
# Usage: ./convert_model.sh your_mnist_model.tflite

if [ $# -eq 0 ]; then
    echo "Usage: $0 <path_to_your_mnist_model.tflite>"
    echo "Example: $0 mnist_model.tflite"
    exit 1
fi

MODEL_FILE="$1"

if [ ! -f "$MODEL_FILE" ]; then
    echo "Error: Model file '$MODEL_FILE' not found!"
    exit 1
fi

echo "Converting $MODEL_FILE to C++ array format..."

# Generate the C++ array
xxd -i "$MODEL_FILE" > temp_model_data.cc

# Extract just the filename without path and extension for variable names
BASENAME=$(basename "$MODEL_FILE" .tflite)

# Create the final model data file
cat > main/person_detect_model_data.cc << EOF
/* Generated from $MODEL_FILE */
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

#include "person_detect_model_data.h"

EOF

# Process the xxd output to use the correct variable names
sed "s/unsigned char.*\[\]/const unsigned char g_mnist_model_data\[\]/" temp_model_data.cc | \
sed "s/unsigned int.*_len/const int g_mnist_model_data_len/" >> main/person_detect_model_data.cc

# Clean up
rm temp_model_data.cc

echo "Model conversion complete!"
echo "File updated: main/person_detect_model_data.cc"
echo ""
echo "Next steps:"
echo "1. Build your project: idf.py build"
echo "2. Flash to ESP32: idf.py flash monitor"
echo ""
echo "Your ESP32 will now detect MNIST digits and display results on the monitor!" 