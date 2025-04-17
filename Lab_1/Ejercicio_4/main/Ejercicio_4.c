#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "xtensa/hal.h"

#define VECTOR_SIZE 20

// Static memory definitions
DRAM_ATTR static int vector_dram[VECTOR_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
DRAM_ATTR static int num_dram = 5;
DRAM_ATTR static int result_dram[VECTOR_SIZE];

IRAM_ATTR static int vector_iram[VECTOR_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
IRAM_ATTR static int num_iram = 5;
IRAM_ATTR static int result_iram[VECTOR_SIZE];

RTC_DATA_ATTR static int vector_rtc[VECTOR_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
RTC_DATA_ATTR static int num_rtc = 5;
RTC_DATA_ATTR static int result_rtc[VECTOR_SIZE];

const __attribute__((section(".rodata"))) int vector_flash[VECTOR_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
const __attribute__((section(".rodata"))) int num_flash = 5;

// Function to multiply vector by scalar
void multiply_vector_scalar(const int* vector, int num, int* result) {
    for (int i = 0; i < VECTOR_SIZE; i++) {
        result[i] = vector[i] * num;
    }
}

// Function to measure execution time and cycles
void measure_performance(void (*func)(const int*, int, int*), const int* vector, int num, int* result, 
                        uint64_t* time_us, uint32_t* cycles) {
    uint64_t start_time = esp_timer_get_time();
    uint32_t start_cycles = xthal_get_ccount();
    
    func(vector, num, result);
    
    uint32_t end_cycles = xthal_get_ccount();
    uint64_t end_time = esp_timer_get_time();
    
    *time_us = end_time - start_time;
    *cycles = end_cycles - start_cycles;
}

void app_main(void) {
    printf("Starting memory access performance measurement...\n\n");

    uint64_t time_us;
    uint32_t cycles;

    // Measure static memory performance
    measure_performance(multiply_vector_scalar, vector_dram, num_dram, result_dram, &time_us, &cycles);
    printf("DRAM - Time: %" PRIu64 " us, Cycles: %" PRIu32 "\n", time_us, cycles);

    measure_performance(multiply_vector_scalar, vector_iram, num_iram, result_iram, &time_us, &cycles);
    printf("IRAM - Time: %" PRIu64 " us, Cycles: %" PRIu32 "\n", time_us, cycles);

    measure_performance(multiply_vector_scalar, vector_rtc, num_rtc, result_rtc, &time_us, &cycles);
    printf("RTC - Time: %" PRIu64 " us, Cycles: %" PRIu32 "\n", time_us, cycles);

    measure_performance(multiply_vector_scalar, vector_flash, num_flash, result_dram, &time_us, &cycles);
    printf("Flash - Time: %" PRIu64 " us, Cycles: %" PRIu32 "\n", time_us, cycles);

    // Dynamic memory allocation and measurement
    printf("\nMeasuring dynamic memory performance...\n");

    // DRAM allocation
    int *vector_dyn_dram = (int *)malloc(VECTOR_SIZE * sizeof(int));
    int *result_dyn_dram = (int *)malloc(VECTOR_SIZE * sizeof(int));
    int *num_dyn_dram = (int *)malloc(sizeof(int));

    if (vector_dyn_dram && result_dyn_dram && num_dyn_dram) {
        *num_dyn_dram = 5;
        for (int i = 0; i < VECTOR_SIZE; i++) {
            vector_dyn_dram[i] = i + 1;
        }

        measure_performance(multiply_vector_scalar, vector_dyn_dram, *num_dyn_dram, result_dyn_dram, &time_us, &cycles);
        printf("Dynamic DRAM - Time: %" PRIu64 " us, Cycles: %" PRIu32 "\n", time_us, cycles);

        free(vector_dyn_dram);
        free(result_dyn_dram);
        free(num_dyn_dram);
    } else {
        printf("Failed to allocate DRAM memory\n");
    }

    // IRAM allocation
    int *vector_dyn_iram = (int *)heap_caps_malloc(VECTOR_SIZE * sizeof(int), MALLOC_CAP_EXEC);
    int *result_dyn_iram = (int *)heap_caps_malloc(VECTOR_SIZE * sizeof(int), MALLOC_CAP_EXEC);
    int *num_dyn_iram = (int *)heap_caps_malloc(sizeof(int), MALLOC_CAP_EXEC);

    if (vector_dyn_iram && result_dyn_iram && num_dyn_iram) {
        *num_dyn_iram = 5;
        for (int i = 0; i < VECTOR_SIZE; i++) {
            vector_dyn_iram[i] = i + 1;
        }

        measure_performance(multiply_vector_scalar, vector_dyn_iram, *num_dyn_iram, result_dyn_iram, &time_us, &cycles);
        printf("Dynamic IRAM - Time: %" PRIu64 " us, Cycles: %" PRIu32 "\n", time_us, cycles);

        heap_caps_free(vector_dyn_iram);
        heap_caps_free(result_dyn_iram);
        heap_caps_free(num_dyn_iram);
    } else {
        printf("Failed to allocate IRAM memory\n");
    }

    // PSRAM allocation
    int *vector_dyn_psram = (int *)heap_caps_malloc(VECTOR_SIZE * sizeof(int), MALLOC_CAP_SPIRAM);
    int *result_dyn_psram = (int *)heap_caps_malloc(VECTOR_SIZE * sizeof(int), MALLOC_CAP_SPIRAM);
    int *num_dyn_psram = (int *)heap_caps_malloc(sizeof(int), MALLOC_CAP_SPIRAM);

    if (vector_dyn_psram && result_dyn_psram && num_dyn_psram) {
        *num_dyn_psram = 5;
        for (int i = 0; i < VECTOR_SIZE; i++) {
            vector_dyn_psram[i] = i + 1;
        }

        measure_performance(multiply_vector_scalar, vector_dyn_psram, *num_dyn_psram, result_dyn_psram, &time_us, &cycles);
        printf("PSRAM - Time: %" PRIu64 " us, Cycles: %" PRIu32 "\n", time_us, cycles);

        heap_caps_free(vector_dyn_psram);
        heap_caps_free(result_dyn_psram);
        heap_caps_free(num_dyn_psram);
    } else {
        printf("Failed to allocate PSRAM memory\n");
    }

    printf("\nMeasurement complete!\n");
} 