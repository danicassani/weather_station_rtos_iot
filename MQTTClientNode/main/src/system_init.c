#include "system_init.h"
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

static const char *TAG = "SYSTEM_INIT";

// WiFi Configuration - Modify according to your network
#define WIFI_SSID      "DIGIFIBRA-2xt5"
#define WIFI_PASS      "uTDcSbN74edQ"

// MQTT Configuration - Modify according to your broker
#define MQTT_BROKER_URI "mqtt://192.168.1.250"

// Values from menuconfig (Kconfig.projbuild)
#ifndef CONFIG_BLINK_GPIO
#define CONFIG_BLINK_GPIO 2
#endif

// Initialize SNTP
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Inicializando SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    
    // Configure timezone (adjust according to your zone)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // Central Europe
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
    const int retry_count = 10;

    while (timeinfo.tm_year < (2023 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Esperando sincronización de tiempo... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    
    if (retry >= retry_count) {
        ESP_LOGW(TAG, "No se pudo sincronizar el tiempo con NTP");
    } else {
        ESP_LOGI(TAG, "Tiempo sincronizado correctamente");
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

esp_err_t init_led(void)
{
    ESP_LOGI(TAG, "Iniciando LED manager...");
    return led_manager_init(CONFIG_BLINK_GPIO);
}

esp_err_t init_wifi(void)
{
    ESP_LOGI(TAG, "Iniciando conexión WiFi...");
    return wifi_manager_init(WIFI_SSID, WIFI_PASS);
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
        ESP_LOGI(TAG, "IP local obtenida: %s", ip_out);
    }
    return ESP_OK;
}

esp_err_t init_mqtt(const char* client_id, const char* ip_address)
{
    ESP_LOGI(TAG, "Iniciando cliente MQTT...");
    return mqtt_manager_init(MQTT_BROKER_URI, client_id, ip_address);
}
