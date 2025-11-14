#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "config.h"
#include "wifi_manager.h"
#include "led_manager.h"
#include "mqtt_manager.h"
#include "system_init.h"
#include "dht11_manager.h"
#include "adc_scanner.h"
#include "hygrometer_manager.h"
#include "cJSON.h"

static const char *TAG = "ESP32_MQTT";

// Helper function to get local IP (kept in main for publish loop)
static bool get_local_ip(char *ip_str, size_t max_len)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        return false;
    }
    
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return false;
    }
    
    snprintf(ip_str, max_len, IPSTR, IP2STR(&ip_info.ip));
    return true;
}

void app_main(void)
{
    // Initialize logging levels first
    init_logging();
    
    ESP_LOGI(TAG, "Starting blink…");

    if (init_nvs() != ESP_OK) {
        fatal_halt("NVS init failed");
    }

    if (init_led() != ESP_OK) {
        fatal_halt("LED manager init failed");
    }

    if (init_wifi() != ESP_OK) {
        fatal_halt("WiFi init failed");
    }

    char ip_address[16] = "N/A";
    if (init_time(ip_address, sizeof(ip_address)) != ESP_OK) {
        fatal_halt("Time sync or IP fetch failed");
    }

    if (init_mqtt(CONFIG_MQTT_CLIENT_ID, ip_address) != ESP_OK) {
        fatal_halt("MQTT init failed");
    }

    // Initialize DHT11 sensor
    if (init_dht11() != ESP_OK) {
        ESP_LOGW(TAG, "DHT11 init failed, continuing without sensor");
    }

    // Initialize ADC scanner
    if (init_adc_scanner() != ESP_OK) {
        ESP_LOGW(TAG, "ADC scanner init failed");
    } else {
        // Run a one-time scan to detect hygrometer GPIO
        ESP_LOGI(TAG, "Scanning ADC channels to detect hygrometer...");
        adc_scan_result_t scan_results[8];
        size_t num_results = 0;
        
        if (adc_scanner_scan(scan_results, &num_results, 8) == ESP_OK) {
            ESP_LOGI(TAG, "ADC scan completed. Check results above.");
        } else {
            ESP_LOGW(TAG, "ADC scan failed");
        }
    }

    // Initialize hygrometer sensor (GPIO32 detected from scan)
    if (init_hygrometer() != ESP_OK) {
        ESP_LOGW(TAG, "Hygrometer init failed, continuing without sensor");
    }

    // Initialize Telnet logger for remote debugging
    if (init_telnet_logger() != ESP_OK) {
        ESP_LOGW(TAG, "Telnet logger init failed, continuing without it");
    } else {
        ESP_LOGI(TAG, "Telnet logger available on telnet://%s:%d", ip_address, CONFIG_TELNET_PORT);
    }

    // Wait a moment for MQTT to connect
    vTaskDelay(pdMS_TO_TICKS(CONFIG_STARTUP_DELAY));

    uint32_t last_dht11_read = 0;    // Track last DHT11 read time
    uint32_t last_hygro_read = 0;    // Track last hygrometer read time

    // Main loop: publish message every 10 seconds
    while (true) {
        // Publish test message with timestamp, client_id and IP
        if (mqtt_manager_is_connected()) {
            ESP_LOGD(TAG, "MQTT connected, preparing to publish...");
            
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // Read DHT11 sensor (respect minimum interval)
            dht11_data_t sensor_data = {0};
            if (current_time - last_dht11_read >= CONFIG_DHT11_READ_INTERVAL) {
                if (dht11_manager_read(&sensor_data) == ESP_OK) {
                    last_dht11_read = current_time;
                } else {
                    ESP_LOGW(TAG, "Failed to read DHT11, using cached data");
                    dht11_manager_get_cached(&sensor_data);  // Try cached data
                }
            } else {
                // Use cached data to avoid polling sensor too frequently
                dht11_manager_get_cached(&sensor_data);
            }
            
            // Read hygrometer sensor (respect minimum interval)
            hygrometer_data_t hygro_data = {0};
            if (current_time - last_hygro_read >= CONFIG_HYGROMETER_READ_INTERVAL) {
                if (hygrometer_manager_read(&hygro_data) == ESP_OK) {
                    last_hygro_read = current_time;
                } else {
                    ESP_LOGW(TAG, "Failed to read hygrometer, using cached data");
                    hygrometer_manager_get_cached(&hygro_data);  // Try cached data
                }
            } else {
                // Use cached data to avoid polling ADC too frequently
                hygrometer_manager_get_cached(&hygro_data);
            }
            
            // Get timestamp
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);

            char timestamp[64];
            strftime(timestamp, sizeof(timestamp), "%d-%m-%Y %H:%M:%S", &timeinfo);

            // Get local IP
            char loop_ip[16] = "N/A";
            get_local_ip(loop_ip, sizeof(loop_ip));
            
            ESP_LOGD(TAG, "IP: %s, Timestamp: %s", loop_ip, timestamp);

            // Build JSON payload
            cJSON *root = cJSON_CreateObject();
            if (!root) {
                ESP_LOGE(TAG, "Failed to create JSON root");
                continue; // skip this cycle
            }

            cJSON_AddStringToObject(root, "client_id", CONFIG_MQTT_CLIENT_ID);
            cJSON_AddStringToObject(root, "ip", loop_ip);
            cJSON_AddStringToObject(root, "timestamp", timestamp);

            // DHT11 fields
            if (sensor_data.valid) {
                cJSON_AddNumberToObject(root, "temperature_c", sensor_data.temperature);
                cJSON_AddNumberToObject(root, "humidity_pct", sensor_data.humidity);
            } else {
                cJSON_AddNullToObject(root, "temperature_c");
                cJSON_AddNullToObject(root, "humidity_pct");
            }

            // Hygrometer fields
            if (hygro_data.valid) {
                cJSON_AddNumberToObject(root, "moisture_pct", hygro_data.moisture_percent);
            } else {
                cJSON_AddNullToObject(root, "moisture_pct");
            }

            // Serialize JSON
            char *json_str = cJSON_PrintUnformatted(root);
            if (!json_str) {
                ESP_LOGE(TAG, "Failed to serialize JSON");
                cJSON_Delete(root);
                continue;
            }

            ESP_LOGD(TAG, "Publishing JSON to topic '%s'...", CONFIG_MQTT_TOPIC);

            // Pulse LED for half a second while sending the message
            led_manager_pulse(CONFIG_LED_PULSE_MS);

            int msg_id = mqtt_manager_publish(CONFIG_MQTT_TOPIC, json_str, CONFIG_MQTT_QOS, 0);
            if (msg_id != -1) {
                ESP_LOGD(TAG, "Message published successfully, msg_id=%d", msg_id);
                ESP_LOGD(TAG, "Message details - Topic: %s, QoS: %d, Length: %d", 
                         CONFIG_MQTT_TOPIC, CONFIG_MQTT_QOS, strlen(json_str));
            } else {
                ESP_LOGE(TAG, "Failed to publish message");
            }

            // Cleanup JSON
            cJSON_free(json_str);
            cJSON_Delete(root);
        } else {
            ESP_LOGW(TAG, "MQTT not connected, waiting for connection...");
        }

        // Wait before next publish
        vTaskDelay(pdMS_TO_TICKS(CONFIG_PUBLISH_INTERVAL));
    }
}
