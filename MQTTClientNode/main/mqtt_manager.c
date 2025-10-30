#include "mqtt_manager.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "MQTT_MANAGER";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_is_connected = false;
static char s_lwt_message[256];  // Buffer for Last Will message

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        s_is_connected = true;
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        s_is_connected = false;
        break;
        
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "Topic: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "Data: %.*s", event->data_len, event->data);
        break;
        
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Error de transporte reportado, errno=%d", event->error_handle->esp_transport_sock_errno);
        }
        break;
        
    default:
        ESP_LOGI(TAG, "Evento MQTT no manejado, id=%d", event->event_id);
        break;
    }
}

esp_err_t mqtt_manager_init(const char *broker_uri, const char *client_id, const char *ip_address)
{
    if (broker_uri == NULL || client_id == NULL || ip_address == NULL) {
        ESP_LOGE(TAG, "Parámetros NULL en mqtt_manager_init");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_mqtt_client != NULL) {
        ESP_LOGW(TAG, "Cliente MQTT ya inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Inicializando cliente MQTT con broker: %s", broker_uri);
    ESP_LOGI(TAG, "Client ID: %s, IP: %s", client_id, ip_address);

    // Build Last Will Testament message
    snprintf(s_lwt_message, sizeof(s_lwt_message),
             "Yo, %s, con IP %s, me he desconectado inesperadamente",
             client_id, ip_address);

    ESP_LOGI(TAG, "Last Will configurado: %s", s_lwt_message);

    // MQTT client configuration with Last Will Testament
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .credentials.client_id = client_id,
        .session.last_will = {
            .topic = "disconnections",
            .msg = s_lwt_message,
            .msg_len = strlen(s_lwt_message),
            .qos = 1,
            .retain = false,
        },
        .session.keepalive = 60,  // Keepalive of 60 seconds
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Error al inicializar cliente MQTT");
        return ESP_FAIL;
    }

    // Register event handler
    esp_err_t err = esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, 
                                                     mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar evento MQTT: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return err;
    }

    // Start MQTT client
    err = esp_mqtt_client_start(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar cliente MQTT: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return err;
    }

    ESP_LOGI(TAG, "Cliente MQTT iniciado correctamente");
    return ESP_OK;
}

esp_err_t mqtt_manager_deinit(void)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGW(TAG, "Cliente MQTT no estaba inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_mqtt_client_stop(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al detener cliente MQTT: %s", esp_err_to_name(err));
    }

    err = esp_mqtt_client_destroy(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al destruir cliente MQTT: %s", esp_err_to_name(err));
        return err;
    }

    s_mqtt_client = NULL;
    s_is_connected = false;

    ESP_LOGI(TAG, "Cliente MQTT desinicializado");
    return ESP_OK;
}

int mqtt_manager_publish(const char *topic, const char *message, int qos, int retain)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Cliente MQTT no inicializado");
        return -1;
    }

    if (topic == NULL || message == NULL) {
        ESP_LOGE(TAG, "Topic o mensaje NULL");
        return -1;
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, message, 
                                          strlen(message), qos, retain);
    
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Error al publicar mensaje en topic: %s", topic);
    } else {
        ESP_LOGI(TAG, "Mensaje publicado en topic '%s', msg_id=%d", topic, msg_id);
    }

    return msg_id;
}

int mqtt_manager_subscribe(const char *topic, int qos)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Cliente MQTT no inicializado");
        return -1;
    }

    if (topic == NULL) {
        ESP_LOGE(TAG, "Topic NULL");
        return -1;
    }

    int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, topic, qos);
    
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Error al suscribirse al topic: %s", topic);
    } else {
        ESP_LOGI(TAG, "Suscrito al topic '%s', msg_id=%d", topic, msg_id);
    }

    return msg_id;
}

int mqtt_manager_unsubscribe(const char *topic)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Cliente MQTT no inicializado");
        return -1;
    }

    if (topic == NULL) {
        ESP_LOGE(TAG, "Topic NULL");
        return -1;
    }

    int msg_id = esp_mqtt_client_unsubscribe(s_mqtt_client, topic);
    
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Error al desuscribirse del topic: %s", topic);
    } else {
        ESP_LOGI(TAG, "Desuscrito del topic '%s', msg_id=%d", topic, msg_id);
    }

    return msg_id;
}

bool mqtt_manager_is_connected(void)
{
    return s_is_connected;
}
