#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "wifi_manager.h"

static const char *TAG = "ESP32_MQTT";

// Configuración WiFi - Modificar según tu red
#define WIFI_SSID      "DIGIFIBRA-2xt5"
#define WIFI_PASS      "uTDcSbN74edQ"

// Configuración MQTT - Modificar según tu broker
#define MQTT_BROKER_URI "mqtt://192.168.1.250"
#define MQTT_TOPIC      "ESP32"
#define MQTT_MESSAGE    "HOLA DESDE ESP32"

static esp_mqtt_client_handle_t mqtt_client = NULL;

// Valores desde menuconfig (Kconfig.projbuild)
#ifndef CONFIG_BLINK_GPIO
#define CONFIG_BLINK_GPIO 2
#endif

#ifndef CONFIG_BLINK_PERIOD_MS
#define CONFIG_BLINK_PERIOD_MS 1000
#endif

static void configure_led_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << CONFIG_BLINK_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config falló: %s", esp_err_to_name(err));
    } else {
        // Arrancamos en LOW para no deslumbrar
        gpio_set_level(CONFIG_BLINK_GPIO, 0);
        ESP_LOGI(TAG, "GPIO %d configurado como salida", CONFIG_BLINK_GPIO);
    }
}

static void blink_task(void *pv)
{
    const TickType_t half_period = pdMS_TO_TICKS(CONFIG_BLINK_PERIOD_MS / 2);
    bool level = false;

    ESP_LOGI(TAG, "Parpadeo: periodo=%d ms (GPIO=%d)", CONFIG_BLINK_PERIOD_MS, CONFIG_BLINK_GPIO);

    while (true) {
        level = !level;
        gpio_set_level(CONFIG_BLINK_GPIO, level);
        vTaskDelay(half_period);
    }
}

// Manejador de eventos MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // Publicar mensaje al conectarse
        int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, MQTT_MESSAGE, 0, 1, 0);
        ESP_LOGI(TAG, "Mensaje publicado exitosamente, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Evento MQTT id:%d", event->event_id);
        break;
    }
}

// Inicialización MQTT
static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
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
    
    // Configurar LED
    configure_led_gpio();

    // Crear tarea de parpadeo
    BaseType_t ok = xTaskCreatePinnedToCore(
        blink_task,        // función
        "blink_task",      // nombre
        2048,              // stack
        NULL,              // parámetro
        5,                 // prioridad
        NULL,              // handle
        tskNO_AFFINITY     // core (o 0/1 según chip)
    );

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "No pude crear blink_task (me he quedao sin RAM del stack).");
    }
    
    // Inicializar WiFi
    ESP_LOGI(TAG, "Iniciando conexión WiFi...");
    esp_err_t wifi_result = wifi_manager_init(WIFI_SSID, WIFI_PASS);
    if (wifi_result != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar WiFi");
        return;
    }
    
    // Inicializar MQTT
    ESP_LOGI(TAG, "Iniciando cliente MQTT...");
    mqtt_app_start();
}
