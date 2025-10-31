#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initialize and connect MQTT client
 * 
 * @param broker_uri MQTT broker URI (e.g.: "mqtt://192.168.1.250")
 * @param client_id Unique client ID
 * @param ip_address Client IP address (for Last Will)
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t mqtt_manager_init(const char *broker_uri, const char *client_id, const char *ip_address);

/**
 * @brief Deinitialize and disconnect MQTT client
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t mqtt_manager_deinit(void);

/**
 * @brief Publish a message to an MQTT topic
 * 
 * @param topic Topic to publish to
 * @param message Message to publish
 * @param qos Quality of Service (0, 1 or 2)
 * @param retain Whether the message should be retained by the broker
 * @return int Published message ID, -1 if failed
 */
int mqtt_manager_publish(const char *topic, const char *message, int qos, int retain);

/**
 * @brief Subscribe to an MQTT topic
 * 
 * @param topic Topic to subscribe to
 * @param qos Quality of Service (0, 1 or 2)
 * @return int Subscription ID, -1 if failed
 */
int mqtt_manager_subscribe(const char *topic, int qos);

/**
 * @brief Unsubscribe from an MQTT topic
 * 
 * @param topic Topic to unsubscribe from
 * @return int Unsubscription ID, -1 if failed
 */
int mqtt_manager_unsubscribe(const char *topic);

/**
 * @brief Check if MQTT client is connected
 * 
 * @return true if connected, false otherwise
 */
bool mqtt_manager_is_connected(void);

#endif // MQTT_MANAGER_H
