#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"
#include "system_init.h"
#include "mqtt_publisher.h"

static const char *TAG = "ESP32_MQTT";

void app_main(void)
{
    // Initialize entire system
    if (init_system() != ESP_OK) {
        fatal_halt("System initialization failed");
    }

    // // Initialize ADC scanner

    // Wait a moment for MQTT to connect
    vTaskDelay(pdMS_TO_TICKS(CONFIG_STARTUP_DELAY));

    // Main loop: publish sensor data periodically
    while (true) {
        mqtt_publish_sensor_data();
        vTaskDelay(pdMS_TO_TICKS(CONFIG_PUBLISH_INTERVAL));
    }
}

    // if (init_adc_scanner() != ESP_OK) {
    //     ESP_LOGW(TAG, "ADC scanner init failed");
    // } else {
    //     // Run a one-time scan to detect hygrometer GPIO
    //     ESP_LOGI(TAG, "Scanning ADC channels to detect hygrometer...");
    //     adc_scan_result_t scan_results[8];
    //     size_t num_results = 0;
        
    //     if (adc_scanner_scan(scan_results, &num_results, 8) == ESP_OK) {
    //         ESP_LOGI(TAG, "ADC scan completed. Check results above.");
    //     } else {
    //         ESP_LOGW(TAG, "ADC scan failed");
    //     }
    // }