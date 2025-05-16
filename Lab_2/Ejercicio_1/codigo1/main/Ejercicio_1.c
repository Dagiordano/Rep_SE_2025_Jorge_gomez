#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED_PIN GPIO_NUM_4
#define BUTTON_PIN GPIO_NUM_0  // Using GPIO0 as button input
#define MAX_COUNTER 10

static int counter = 0;
static int hello_count = 0;

void button_task(void *pvParameter) {
    // Configure button pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    int last_button_state = 1;  // Button is pulled up, so 1 is not pressed
    int current_button_state;

    while (1) {
        current_button_state = gpio_get_level(BUTTON_PIN);
        
        // Detect falling edge (button press)
        if (last_button_state == 1 && current_button_state == 0) {
            counter++;
            printf("Counter increased to: %d\n", counter);
        }
        
        last_button_state = current_button_state;
        vTaskDelay(pdMS_TO_TICKS(50));  // Debounce delay
    }
}

void hello_task(void *pvParameter) {
    while (1) {
        printf("Hello World from FreeRTOS!\n");
        hello_count++;
        
        if (hello_count >= MAX_COUNTER) {
            hello_count = 0;
            counter = 0;
            printf("Counter reset!\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // 2 segundos
    }
}

void led_blink_task(void *pvParameter) {
    // Configure LED pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    while (1) {
        // Blink frequency is inversely proportional to counter value
        // Minimum delay of 100ms, maximum of 1000ms
        int delay_ms = 1000 - (counter * 100);
        if (delay_ms < 100) delay_ms = 100;
        
        gpio_set_level(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        gpio_set_level(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void app_main() {
    xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);
    xTaskCreate(&led_blink_task, "led_blink_task", 2048, NULL, 5, NULL);
    xTaskCreate(&button_task, "button_task", 2048, NULL, 5, NULL);
}
