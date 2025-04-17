#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "xtensa/hal.h"
#include "esp_cpu.h"

int var_1 = 233;
int var_2 = 128;

// Function to perform operations and measure performance
void app_main(void)
{
    // Variables for results
    int result_0, result_1, result_2, result_3, result_4;
    
    // Variables for timing and performance measurement
    int64_t start_time, end_time;
    uint32_t start_cycles, end_cycles;
    
    // Perform operations X times with multiple runs for better accuracy
    const int X = 70000;
    const int RUNS = 5;  // Number of test runs
    
    printf("\nRunning %d tests with %d iterations each:\n", RUNS, X);
    
    // Variables for overall measurements
    int64_t total_time = 0;
    uint32_t total_cycles = 0;
    
    // Variables for individual operation measurements
    uint32_t total_cycles_add = 0;
    uint32_t total_cycles_add_const = 0;
    uint32_t total_cycles_mod = 0;
    uint32_t total_cycles_mul = 0;
    uint32_t total_cycles_div = 0;
    
    for(int run = 0; run < RUNS; run++) {
        // Measure all operations together
        start_time = esp_timer_get_time();
        start_cycles = esp_cpu_get_cycle_count();
        
        for(int i = 0; i < X; i++) {
            result_0 = var_1 + var_2;
            result_1 = var_1 + 10;
            result_2 = var_1 % var_2;
            result_3 = var_1 * var_2;
            result_4 = var_1 / var_2;
        }
        
        end_time = esp_timer_get_time();
        end_cycles = esp_cpu_get_cycle_count();
        
        int64_t run_time = end_time - start_time;
        uint32_t run_cycles = end_cycles - start_cycles;
        
        total_time += run_time;
        total_cycles += run_cycles;
        
        printf("Run %d: Time: %" PRId64 " us, Cycles: %" PRIu32 "\n", 
               run + 1, run_time, run_cycles);
        
        // Measure individual operations
        // Addition
        start_cycles = esp_cpu_get_cycle_count();
        for(int i = 0; i < X; i++) {
            result_0 = var_1 + var_2;
        }
        end_cycles = esp_cpu_get_cycle_count();
        total_cycles_add += (end_cycles - start_cycles);
        
        // Addition with constant
        start_cycles = esp_cpu_get_cycle_count();
        for(int i = 0; i < X; i++) {
            result_1 = var_1 + 10;
        }
        end_cycles = esp_cpu_get_cycle_count();
        total_cycles_add_const += (end_cycles - start_cycles);
        
        // Modulo
        start_cycles = esp_cpu_get_cycle_count();
        for(int i = 0; i < X; i++) {
            result_2 = var_1 % var_2;
        }
        end_cycles = esp_cpu_get_cycle_count();
        total_cycles_mod += (end_cycles - start_cycles);
        
        // Multiplication
        start_cycles = esp_cpu_get_cycle_count();
        for(int i = 0; i < X; i++) {
            result_3 = var_1 * var_2;
        }
        end_cycles = esp_cpu_get_cycle_count();
        total_cycles_mul += (end_cycles - start_cycles);
        
        // Division
        start_cycles = esp_cpu_get_cycle_count();
        for(int i = 0; i < X; i++) {
            result_4 = var_1 / var_2;
        }
        end_cycles = esp_cpu_get_cycle_count();
        total_cycles_div += (end_cycles - start_cycles);
    }
    
    // Calculate overall averages
    float avg_time_us = (float)total_time / RUNS;
    float avg_cycles = (float)total_cycles / RUNS;
    float avg_time_per_op = avg_time_us / (X * 5); // 5 operations per iteration
    float avg_cycles_per_op = avg_cycles / (X * 5);
    
    // Calculate individual operation averages
    float avg_cycles_add = (float)total_cycles_add / RUNS;
    float avg_cycles_add_const = (float)total_cycles_add_const / RUNS;
    float avg_cycles_mod = (float)total_cycles_mod / RUNS;
    float avg_cycles_mul = (float)total_cycles_mul / RUNS;
    float avg_cycles_div = (float)total_cycles_div / RUNS;
    
    printf("\nOverall Performance Measurements:\n");
    printf("Average time per run: %.2f us\n", avg_time_us);
    printf("Average cycles per run: %.2f\n", avg_cycles);
    printf("Average time per operation: %.3f us\n", avg_time_per_op);
    printf("Average cycles per operation: %.3f\n", avg_cycles_per_op);
    
    printf("\nIndividual Operation Cycle Counts:\n");
    printf("Addition (var_1 + var_2): %.2f cycles\n", avg_cycles_add / X);
    printf("Addition with constant (var_1 + 10): %.2f cycles\n", avg_cycles_add_const / X);
    printf("Modulo (var_1 %% var_2): %.2f cycles\n", avg_cycles_mod / X);
    printf("Multiplication (var_1 * var_2): %.2f cycles\n", avg_cycles_mul / X);
    printf("Division (var_1 / var_2): %.2f cycles\n", avg_cycles_div / X);
    
    // Print operation results (from last iteration)
    printf("\nOperation Results:\n");
    printf("result_0 (var_1 + var_2): %d\n", result_0);
    printf("result_1 (var_1 + 10): %d\n", result_1);
    printf("result_2 (var_1 %% var_2): %d\n", result_2);
    printf("result_3 (var_1 * var_2): %d\n", result_3);
    printf("result_4 (var_1 / var_2): %d\n", result_4);
}
