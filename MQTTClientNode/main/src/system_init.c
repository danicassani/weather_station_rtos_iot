#include "system_init.h"
#include "config.h"
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include "led_manager.h"
#include "mqtt_manager.h"
#include "telnet_logger.h"
#include "dht11_manager.h"
#include "adc_scanner.h"
#include "hygrometer_manager.h"

static const char *TAG = "SYSTEM_INIT";

// Configure log levels for all modules
static void configure_log_levels(void)
{
    // Set global default log level
    esp_log_level_set("*", CONFIG_APP_LOG_LEVEL);
    
    // Set per-module log levels
    esp_log_level_set("WIFI_MANAGER", CONFIG_LOG_LEVEL_WIFI);
    esp_log_level_set("MQTT_MANAGER", CONFIG_LOG_LEVEL_MQTT);
    esp_log_level_set("LED_MANAGER", CONFIG_LOG_LEVEL_LED);
    esp_log_level_set("TELNET_LOGGER", CONFIG_LOG_LEVEL_TELNET);
    esp_log_level_set("SYSTEM_INIT", CONFIG_LOG_LEVEL_INIT);
    esp_log_level_set("ESP32_MQTT", CONFIG_LOG_LEVEL_MAIN);
    esp_log_level_set("DHT11_MANAGER", CONFIG_LOG_LEVEL_DHT11);
    esp_log_level_set("ADC_SCANNER", CONFIG_LOG_LEVEL_ADC);
    esp_log_level_set("HYGROMETER_MANAGER", CONFIG_LOG_LEVEL_HYGRO);
    
    ESP_LOGI(TAG, "Log levels configured - Global: %d", CONFIG_APP_LOG_LEVEL);
}

// Initialize SNTP
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, CONFIG_NTP_SERVER);
    esp_sntp_init();
    
    // Configure timezone
    setenv("TZ", CONFIG_TIMEZONE, 1);
    tzset();
}

// Get local IP
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

// Wait for NTP synchronization
static void wait_for_time_sync(void)
{
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;

    while (timeinfo.tm_year < (2023 - 1900) && ++retry < CONFIG_NTP_SYNC_TIMEOUT) {
        ESP_LOGI(TAG, "Waiting for time synchronization... (%d/%d)", retry, CONFIG_NTP_SYNC_TIMEOUT);
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    
    if (retry >= CONFIG_NTP_SYNC_TIMEOUT) {
        ESP_LOGW(TAG, "Failed to synchronize time with NTP");
    } else {
        ESP_LOGI(TAG, "Time synchronized successfully");
    }
}

void fatal_halt(const char* reason)
{
    if (reason) {
        ESP_LOGE(TAG, "FATAL: %s", reason);
    }
    // Sleep forever to halt this task without reboot
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t init_logging(void)
{
    configure_log_levels();
    return ESP_OK;
}

esp_err_t init_led(void)
{
    ESP_LOGI(TAG, "Starting LED manager...");
    return led_manager_init(CONFIG_BLINK_GPIO);
}

esp_err_t init_wifi(void)
{
    ESP_LOGI(TAG, "Starting WiFi connection...");
    return wifi_manager_init(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
}

esp_err_t init_time(char* ip_out, size_t ip_out_len)
{
    // Initialize and sync time via SNTP
    initialize_sntp();
    wait_for_time_sync();
    // Fetch local IP for LWT and logs
    if (ip_out && ip_out_len > 0) {
        if (!get_local_ip(ip_out, ip_out_len)) {
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Local IP: %s", ip_out);
    }
    return ESP_OK;
}

esp_err_t init_mqtt(const char* client_id, const char* ip_address)
{
    ESP_LOGI(TAG, "Starting MQTT client...");
    return mqtt_manager_init(CONFIG_MQTT_BROKER_URI, client_id, ip_address);
}

esp_err_t init_telnet_logger(void)
{
#if CONFIG_TELNET_ENABLED
    ESP_LOGI(TAG, "Starting Telnet server on port %d...", CONFIG_TELNET_PORT);
    return telnet_logger_init(CONFIG_TELNET_PORT);
#else
    ESP_LOGI(TAG, "Telnet logger disabled in configuration");
    return ESP_OK;
#endif
}

esp_err_t init_dht11(void)
{
    ESP_LOGI(TAG, "Initializing DHT11 sensor...");
    
    // Pass -1 to auto-scan all GPIOs when CONFIG_DHT11_AUTO_SCAN is true (1),
    // otherwise use CONFIG_DHT11_GPIO for a fixed pin.
#if CONFIG_DHT11_AUTO_SCAN
    return dht11_manager_init(-1);  // Auto-scan mode
#else
    return dht11_manager_init(CONFIG_DHT11_GPIO);  // Use configured GPIO
#endif
}

esp_err_t init_adc_scanner(void)
{
    ESP_LOGI(TAG, "Initializing ADC scanner...");
    return adc_scanner_init();
}

esp_err_t init_hygrometer(void)
{
    ESP_LOGI(TAG, "Initializing hygrometer sensor...");
    return hygrometer_manager_init(CONFIG_HYGROMETER_GPIO);
}
