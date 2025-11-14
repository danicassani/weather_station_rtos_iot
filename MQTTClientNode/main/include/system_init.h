#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include "esp_err.h"

/**
 * @brief Initialize NVS (Non-Volatile Storage)
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t init_nvs(void);

/**
 * @brief Initialize logging system with configured levels
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t init_logging(void);

/**
 * @brief Initialize LED manager
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t init_led(void);

/**
 * @brief Initialize WiFi connection
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t init_wifi(void);

/**
 * @brief Initialize SNTP time sync and get local IP
 * 
 * @param ip_out Buffer to store local IP address
 * @param ip_out_len Size of ip_out buffer
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t init_time(char* ip_out, size_t ip_out_len);

/**
 * @brief Initialize MQTT client with Last Will Testament
 * 
 * @param client_id MQTT client ID
 * @param ip_address Local IP address for LWT message
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t init_mqtt(const char* client_id, const char* ip_address);

/**
 * @brief Initialize Telnet logger server
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t init_telnet_logger(void);

/**
 * @brief Initialize DHT11 sensor
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t init_dht11(void);

/**
 * @brief Initialize ADC scanner
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t init_adc_scanner(void);

/**
 * @brief Initialize hygrometer sensor
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t init_hygrometer(void);

/**
 * @brief Fatal halt: log reason and stop main task forever
 * 
 * @param reason Error message to log
 */
void fatal_halt(const char* reason);

#endif // SYSTEM_INIT_H
