#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "wifi_manager.h"
#include "led_manager.h"
#include "mqtt_manager.h"
#include "system_init.h"

static const char *TAG = "ESP32_MQTT";

// MQTT Configuration - Modify according to your broker
#define MQTT_CLIENT_ID  "ESP32_NODE_001"
#define MQTT_TOPIC      "ESP32"

#define LED_PULSE_DURATION_MS 500  // Half a second

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
    ESP_LOGI(TAG, "Arrancando blink…");

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

    if (init_mqtt(MQTT_CLIENT_ID, ip_address) != ESP_OK) {
        fatal_halt("MQTT init failed");
    }

    // Wait a moment for MQTT to connect
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Main loop: publish message every 10 seconds
    while (true) {
        // Publish test message with timestamp, client_id and IP
        if (mqtt_manager_is_connected()) {
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

            // Build complete message
            char message[256];
            snprintf(message, sizeof(message),
                     "ESP32 | Client: %s | IP: %s | Timestamp: %s",
                     MQTT_CLIENT_ID, loop_ip, timestamp);

            ESP_LOGI(TAG, "Publicando mensaje en topic '%s'...", MQTT_TOPIC);
            ESP_LOGI(TAG, "Mensaje: %s", message);

            // Pulse LED for half a second while sending the message
            led_manager_pulse(LED_PULSE_DURATION_MS);

            int msg_id = mqtt_manager_publish(MQTT_TOPIC, message, 1, 0);
            if (msg_id != -1) {
                ESP_LOGI(TAG, "Mensaje publicado exitosamente, msg_id=%d", msg_id);
            }
        } else {
            ESP_LOGW(TAG, "MQTT no conectado, esperando conexión...");
        }

        // Wait 10 seconds before next send
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
