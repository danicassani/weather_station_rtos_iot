#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Inicializa y conecta a la red WiFi
 * 
 * @param ssid SSID de la red WiFi
 * @param password Contraseña de la red WiFi
 * @return esp_err_t ESP_OK si la conexión fue exitosa, error en caso contrario
 */
esp_err_t wifi_manager_init(const char *ssid, const char *password);

/**
 * @brief Desconecta y desinicializa el WiFi
 * 
 * @return esp_err_t ESP_OK si fue exitoso
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief Verifica si el WiFi está conectado
 * 
 * @return true si está conectado, false en caso contrario
 */
bool wifi_manager_is_connected(void);

#endif // WIFI_MANAGER_H
