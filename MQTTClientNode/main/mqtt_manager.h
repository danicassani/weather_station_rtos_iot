#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Inicializa y conecta el cliente MQTT
 * 
 * @param broker_uri URI del broker MQTT (ej: "mqtt://192.168.1.250")
 * @param client_id ID único del cliente
 * @param ip_address Dirección IP del cliente (para Last Will)
 * @return esp_err_t ESP_OK si fue exitoso
 */
esp_err_t mqtt_manager_init(const char *broker_uri, const char *client_id, const char *ip_address);

/**
 * @brief Desinicializa y desconecta el cliente MQTT
 * 
 * @return esp_err_t ESP_OK si fue exitoso
 */
esp_err_t mqtt_manager_deinit(void);

/**
 * @brief Publica un mensaje en un topic MQTT
 * 
 * @param topic Topic donde publicar
 * @param message Mensaje a publicar
 * @param qos Calidad de servicio (0, 1 o 2)
 * @param retain Si el mensaje debe ser retenido por el broker
 * @return int ID del mensaje publicado, -1 si falla
 */
int mqtt_manager_publish(const char *topic, const char *message, int qos, int retain);

/**
 * @brief Se suscribe a un topic MQTT
 * 
 * @param topic Topic al que suscribirse
 * @param qos Calidad de servicio (0, 1 o 2)
 * @return int ID de la suscripción, -1 si falla
 */
int mqtt_manager_subscribe(const char *topic, int qos);

/**
 * @brief Se desuscribe de un topic MQTT
 * 
 * @param topic Topic del que desuscribirse
 * @return int ID de la desuscripción, -1 si falla
 */
int mqtt_manager_unsubscribe(const char *topic);

/**
 * @brief Verifica si el cliente MQTT está conectado
 * 
 * @return true si está conectado, false en caso contrario
 */
bool mqtt_manager_is_connected(void);

#endif // MQTT_MANAGER_H
