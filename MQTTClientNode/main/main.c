#include <stdio.h>
#include <string.h>
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

static const char *TAG = "ESP32_MQTT";

// Configuración WiFi - Modificar según tu red
#define WIFI_SSID      "DIGIFIBRA-2xt5"
#define WIFI_PASS      "uTDcSbN74edQ"

// Configuración MQTT - Modificar según tu broker
#define MQTT_BROKER_URI "mqtt://192.168.1.250"
#define MQTT_CLIENT_ID  "ESP32_NODE_001"
#define MQTT_TOPIC      "ESP32"

// Valores desde menuconfig (Kconfig.projbuild)
#ifndef CONFIG_BLINK_GPIO
#define CONFIG_BLINK_GPIO 2
#endif

#define LED_PULSE_DURATION_MS 500  // Medio segundo

// Inicializar SNTP
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Inicializando SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    
    // Configurar zona horaria (ajusta según tu zona)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // Europa Central
    tzset();
}

// Obtener IP local
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

// Esperar sincronización NTP
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

void app_main(void)
{
    ESP_LOGI(TAG, "Arrancando blink…");
    
    // Inicializar NVS (requerido para WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Inicializar LED manager (sin timer automático)
    ESP_LOGI(TAG, "Iniciando LED manager...");
    esp_err_t led_result = led_manager_init(CONFIG_BLINK_GPIO);
    if (led_result != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar LED manager");
        return;
    }
    
    // Inicializar WiFi
    ESP_LOGI(TAG, "Iniciando conexión WiFi...");
    esp_err_t wifi_result = wifi_manager_init(WIFI_SSID, WIFI_PASS);
    if (wifi_result != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar WiFi");
        return;
    }
    
    // Inicializar y sincronizar SNTP
    initialize_sntp();
    wait_for_time_sync();
    
    // Obtener IP local para Last Will
    char ip_address[16] = "N/A";
    get_local_ip(ip_address, sizeof(ip_address));
    ESP_LOGI(TAG, "IP local obtenida: %s", ip_address);
    
    // Inicializar MQTT con Last Will Testament
    ESP_LOGI(TAG, "Iniciando cliente MQTT...");
    esp_err_t mqtt_result = mqtt_manager_init(MQTT_BROKER_URI, MQTT_CLIENT_ID, ip_address);
    if (mqtt_result != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar MQTT");
        return;
    }
    
    // Esperar un momento para que MQTT se conecte
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Bucle principal: publicar mensaje cada 10 segundos
    while (true) {
        // Publicar mensaje de prueba con timestamp, client_id e IP
        if (mqtt_manager_is_connected()) {
            // Obtener timestamp
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            
            char timestamp[64];
            strftime(timestamp, sizeof(timestamp), "%d-%m-%Y %H:%M:%S", &timeinfo);
            
            // Obtener IP local
            char ip_address[16] = "N/A";
            get_local_ip(ip_address, sizeof(ip_address));
            
            // Construir mensaje completo
            char message[256];
            snprintf(message, sizeof(message), 
                     "ESP32 | Client: %s | IP: %s | Timestamp: %s",
                     MQTT_CLIENT_ID, ip_address, timestamp);
            
            ESP_LOGI(TAG, "Publicando mensaje en topic '%s'...", MQTT_TOPIC);
            ESP_LOGI(TAG, "Mensaje: %s", message);
            
            // Pulsar LED por medio segundo mientras se envía el mensaje
            led_manager_pulse(LED_PULSE_DURATION_MS);
            
            int msg_id = mqtt_manager_publish(MQTT_TOPIC, message, 1, 0);
            if (msg_id != -1) {
                ESP_LOGI(TAG, "Mensaje publicado exitosamente, msg_id=%d", msg_id);
            }
        } else {
            ESP_LOGW(TAG, "MQTT no conectado, esperando conexión...");
        }
        
        // Esperar 10 segundos antes del siguiente envío
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
