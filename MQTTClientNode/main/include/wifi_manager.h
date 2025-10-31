#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initialize and connect to WiFi network
 * 
 * @param ssid WiFi network SSID
 * @param password WiFi network password
 * @return esp_err_t ESP_OK if connection was successful, error otherwise
 */
esp_err_t wifi_manager_init(const char *ssid, const char *password);

/**
 * @brief Disconnect and deinitialize WiFi
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief Check if WiFi is connected
 * 
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

#endif // WIFI_MANAGER_H
