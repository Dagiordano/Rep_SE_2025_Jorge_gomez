# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Add esp32-camera component
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pruebacam)
