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

    // Initialize Telnet logger for remote debugging
    if (init_telnet_logger() != ESP_OK) {
        ESP_LOGW(TAG, "Telnet logger init failed, continuing without it");
    } else {
        ESP_LOGI(TAG, "Telnet logger available on telnet://%s:%d", ip_address, CONFIG_TELNET_PORT);
    }

    // Wait a moment for MQTT to connect
    vTaskDelay(pdMS_TO_TICKS(CONFIG_STARTUP_DELAY));

    // Main loop: publish message every 10 seconds
    while (true) {
        // Publish test message with timestamp, client_id and IP
        if (mqtt_manager_is_connected()) {
            ESP_LOGD(TAG, "MQTT connected, preparing to publish...");
            
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

            // Build complete message
            char message[256];
            snprintf(message, sizeof(message),
                     "ESP32 | Client: %s | IP: %s | Timestamp: %s",
                     CONFIG_MQTT_CLIENT_ID, loop_ip, timestamp);

            ESP_LOGD(TAG, "Publishing message '%s' to topic '%s'...", message, CONFIG_MQTT_TOPIC);

            // Pulse LED for half a second while sending the message
            led_manager_pulse(CONFIG_LED_PULSE_MS);

            int msg_id = mqtt_manager_publish(CONFIG_MQTT_TOPIC, message, CONFIG_MQTT_QOS, 0);
            if (msg_id != -1) {
                ESP_LOGI(TAG, "Message published successfully, msg_id=%d", msg_id);
                ESP_LOGD(TAG, "Message details - Topic: %s, QoS: %d, Length: %d", 
                         CONFIG_MQTT_TOPIC, CONFIG_MQTT_QOS, strlen(message));
            } else {
                ESP_LOGE(TAG, "Failed to publish message");
            }
        } else {
            ESP_LOGW(TAG, "MQTT not connected, waiting for connection...");
        }

        // Wait before next publish
        vTaskDelay(pdMS_TO_TICKS(CONFIG_PUBLISH_INTERVAL));
    }
}
