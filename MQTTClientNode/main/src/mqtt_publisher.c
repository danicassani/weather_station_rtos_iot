#include "mqtt_publisher.h"
#include "config.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "dht11_manager.h"
#include "hygrometer_manager.h"
#include "mqtt_manager.h"
#include "led_manager.h"
#include "cJSON.h"
#include <time.h>
#include <string.h>
#include "freertos/FreeRTOS.h"

static const char *TAG = "MQTT_PUBLISHER";

// State tracking for sensor read intervals
static uint32_t last_dht11_read = 0;
static uint32_t last_hygro_read = 0;

// Helper: Get local IP address
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

// Helper: Get current timestamp as formatted string
static void get_timestamp(char *timestamp_str, size_t max_len)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(timestamp_str, max_len, "%d-%m-%Y %H:%M:%S", &timeinfo);
}

// Helper: Read DHT11 sensor with interval control
static void read_dht11_sensor(dht11_data_t *sensor_data)
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if (current_time - last_dht11_read >= CONFIG_DHT11_READ_INTERVAL) {
        if (dht11_manager_read(sensor_data) == ESP_OK) {
            last_dht11_read = current_time;
            ESP_LOGD(TAG, "DHT11 read: Temp=%.1f°C, Humidity=%.1f%%", 
                     sensor_data->temperature, sensor_data->humidity);
        } else {
            ESP_LOGW(TAG, "Failed to read DHT11, using cached data");
            dht11_manager_get_cached(sensor_data);
        }
    } else {
        // Use cached data to avoid polling too frequently
        dht11_manager_get_cached(sensor_data);
    }
}

// Helper: Read hygrometer sensor with interval control
static void read_hygrometer_sensor(hygrometer_data_t *hygro_data)
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if (current_time - last_hygro_read >= CONFIG_HYGROMETER_READ_INTERVAL) {
        if (hygrometer_manager_read(hygro_data) == ESP_OK) {
            last_hygro_read = current_time;
            ESP_LOGD(TAG, "Hygrometer read: Moisture=%.1f%%", hygro_data->moisture_percent);
        } else {
            ESP_LOGW(TAG, "Failed to read hygrometer, using cached data");
            hygrometer_manager_get_cached(hygro_data);
        }
    } else {
        // Use cached data to avoid polling ADC too frequently
        hygrometer_manager_get_cached(hygro_data);
    }
}

// Helper: Build JSON payload from sensor data
static char* build_json_payload(const char *client_id, const char *ip, const char *timestamp,
                                 const dht11_data_t *dht11, const hygrometer_data_t *hygro)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON root");
        return NULL;
    }

    // Add metadata
    cJSON_AddStringToObject(root, "client_id", client_id);
    cJSON_AddStringToObject(root, "ip", ip);
    cJSON_AddStringToObject(root, "timestamp", timestamp);

    // Add DHT11 data
    if (dht11->valid) {
        cJSON_AddNumberToObject(root, "temperature_c", dht11->temperature);
        cJSON_AddNumberToObject(root, "humidity_pct", dht11->humidity);
    } else {
        cJSON_AddNullToObject(root, "temperature_c");
        cJSON_AddNullToObject(root, "humidity_pct");
    }

    // Add hygrometer data
    if (hygro->valid) {
        cJSON_AddNumberToObject(root, "moisture_pct", hygro->moisture_percent);
    } else {
        cJSON_AddNullToObject(root, "moisture_pct");
    }

    // Serialize to string
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}

esp_err_t mqtt_publish_sensor_data(void)
{
    // Check MQTT connection
    if (!mqtt_manager_is_connected()) {
        ESP_LOGW(TAG, "MQTT not connected, skipping publish");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Starting sensor data collection and publish...");

    // Read sensors
    dht11_data_t dht11_data = {0};
    hygrometer_data_t hygro_data = {0};
    read_dht11_sensor(&dht11_data);
    read_hygrometer_sensor(&hygro_data);

    // Get metadata
    char ip_address[16] = "N/A";
    char timestamp[64];
    get_local_ip(ip_address, sizeof(ip_address));
    get_timestamp(timestamp, sizeof(timestamp));

    ESP_LOGD(TAG, "IP: %s, Timestamp: %s", ip_address, timestamp);

    // Build JSON payload
    char *json_str = build_json_payload(CONFIG_MQTT_CLIENT_ID, ip_address, timestamp,
                                         &dht11_data, &hygro_data);
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to build JSON payload");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "Publishing JSON to topic '%s': %s", CONFIG_MQTT_TOPIC, json_str);

    // Pulse LED while publishing
    led_manager_pulse(CONFIG_LED_PULSE_MS);

    // Publish to MQTT
    int msg_id = mqtt_manager_publish(CONFIG_MQTT_TOPIC, json_str, CONFIG_MQTT_QOS, 0);
    
    esp_err_t result = ESP_OK;
    if (msg_id != -1) {
        ESP_LOGI(TAG, "Message published successfully, msg_id=%d", msg_id);
        ESP_LOGD(TAG, "Message details - Topic: %s, QoS: %d, Length: %d", 
                 CONFIG_MQTT_TOPIC, CONFIG_MQTT_QOS, strlen(json_str));
    } else {
        ESP_LOGE(TAG, "Failed to publish message");
        result = ESP_FAIL;
    }

    // Cleanup
    cJSON_free(json_str);

    return result;
}
