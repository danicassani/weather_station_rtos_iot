#include "mqtt_manager.h"
#include "config.h"
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
            ESP_LOGE(TAG, "Transport error reported, errno=%d", event->error_handle->esp_transport_sock_errno);
        }
        break;
        
    default:
        ESP_LOGI(TAG, "Unhandled MQTT event, id=%d", event->event_id);
        break;
    }
}

esp_err_t mqtt_manager_init(const char *broker_uri, const char *client_id, const char *ip_address)
{
    if (broker_uri == NULL || client_id == NULL || ip_address == NULL) {
        ESP_LOGE(TAG, "NULL parameters in mqtt_manager_init");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_mqtt_client != NULL) {
        ESP_LOGW(TAG, "MQTT client already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Initializing MQTT client with broker: %s", broker_uri);
    ESP_LOGI(TAG, "Client ID: %s, IP: %s", client_id, ip_address);

    // Build Last Will Testament message
    snprintf(s_lwt_message, sizeof(s_lwt_message),
             "Client %s with IP %s disconnected unexpectedly",
             client_id, ip_address);

    ESP_LOGI(TAG, "Last Will configured: %s", s_lwt_message);

    // MQTT client configuration with Last Will Testament
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .credentials.client_id = client_id,
        .session.last_will = {
            .topic = CONFIG_MQTT_LWT_TOPIC,
            .msg = s_lwt_message,
            .msg_len = strlen(s_lwt_message),
            .qos = CONFIG_MQTT_QOS,
            .retain = true,
        },
        .session.keepalive = CONFIG_MQTT_KEEPALIVE,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    // Register event handler
    esp_err_t err = esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, 
                                                     mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return err;
    }

    // Start MQTT client
    err = esp_mqtt_client_start(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return err;
    }

    ESP_LOGI(TAG, "MQTT client started successfully");
    return ESP_OK;
}

esp_err_t mqtt_manager_deinit(void)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGW(TAG, "MQTT client was not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_mqtt_client_stop(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop MQTT client: %s", esp_err_to_name(err));
    }

    err = esp_mqtt_client_destroy(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to destroy MQTT client: %s", esp_err_to_name(err));
        return err;
    }

    s_mqtt_client = NULL;
    s_is_connected = false;

    ESP_LOGI(TAG, "MQTT client deinitialized");
    return ESP_OK;
}

int mqtt_manager_publish(const char *topic, const char *message, int qos, int retain)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return -1;
    }

    if (topic == NULL || message == NULL) {
        ESP_LOGE(TAG, "Topic or message is NULL");
        return -1;
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, message, 
                                          strlen(message), qos, retain);
    
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Failed to publish message to topic: %s", topic);
    } else {
        ESP_LOGI(TAG, "Message published to topic '%s', msg_id=%d", topic, msg_id);
    }

    return msg_id;
}

int mqtt_manager_subscribe(const char *topic, int qos)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return -1;
    }

    if (topic == NULL) {
        ESP_LOGE(TAG, "Topic is NULL");
        return -1;
    }

    int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, topic, qos);
    
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
    } else {
        ESP_LOGI(TAG, "Subscribed to topic '%s', msg_id=%d", topic, msg_id);
    }

    return msg_id;
}

int mqtt_manager_unsubscribe(const char *topic)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return -1;
    }

    if (topic == NULL) {
        ESP_LOGE(TAG, "Topic is NULL");
        return -1;
    }

    int msg_id = esp_mqtt_client_unsubscribe(s_mqtt_client, topic);
    
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Failed to unsubscribe from topic: %s", topic);
    } else {
        ESP_LOGI(TAG, "Unsubscribed from topic '%s', msg_id=%d", topic, msg_id);
    }

    return msg_id;
}

bool mqtt_manager_is_connected(void)
{
    return s_is_connected;
}
