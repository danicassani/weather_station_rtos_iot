#ifndef TELNET_LOGGER_H
#define TELNET_LOGGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initialize Telnet logger server
 * 
 * Starts a TCP server on the specified port that accepts Telnet connections
 * and redirects ESP_LOG* output to all connected clients.
 * 
 * @param port TCP port for Telnet server (default: 23)
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t telnet_logger_init(uint16_t port);

/**
 * @brief Deinitialize Telnet logger server
 * 
 * Stops the server and closes all client connections
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t telnet_logger_deinit(void);

/**
 * @brief Check if any Telnet client is connected
 * 
 * @return true if at least one client is connected, false otherwise
 */
bool telnet_logger_has_clients(void);

/**
 * @brief Get number of connected Telnet clients
 * 
 * @return int Number of active connections
 */
int telnet_logger_get_client_count(void);

#endif // TELNET_LOGGER_H
