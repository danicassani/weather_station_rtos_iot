#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "driver/gpio.h"

static const char *TAG = "ESP32_MQTT";

// Configuración WiFi - Modificar según tu red
#define WIFI_SSID      "DIGIFIBRA-2xt5"
#define WIFI_PASS      "uTDcSbN74edQ"

// Configuración MQTT - Modificar según tu broker
#define MQTT_BROKER_URI "mqtt://192.168.1.131"  // Broker público de prueba
#define MQTT_TOPIC      "ESP32"
#define MQTT_MESSAGE    "HOLA DESDE ESP32"

// Event group para sincronización WiFi
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int wifi_retry_num = 0;
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

// Manejador de eventos WiFi
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_num < 10) {
            esp_wifi_connect();
            wifi_retry_num++;
            ESP_LOGI(TAG, "Reintentando conexión WiFi...");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "Fallo al conectar a WiFi");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP obtenida:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Inicialización WiFi
static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Inicialización WiFi completada");

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado a SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Fallo al conectar a SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Evento inesperado");
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
    wifi_init_sta();
    
    // Inicializar MQTT
    ESP_LOGI(TAG, "Iniciando cliente MQTT...");
    mqtt_app_start();
}
