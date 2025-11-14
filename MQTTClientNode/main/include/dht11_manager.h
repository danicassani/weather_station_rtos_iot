#ifndef DHT11_MANAGER_H
#define DHT11_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief DHT11 sensor data structure
 */
typedef struct {
    float temperature;  // Temperature in Celsius
    float humidity;     // Relative humidity in %
    bool valid;         // True if data is valid
} dht11_data_t;

/**
 * @brief Initialize DHT11 sensor
 * 
 * @param gpio_num GPIO pin connected to DHT11 data pin
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t dht11_manager_init(int gpio_num);

/**
 * @brief Deinitialize DHT11 sensor
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t dht11_manager_deinit(void);

/**
 * @brief Read temperature and humidity from DHT11 sensor
 * 
 * This function performs a complete read cycle:
 * - Sends start signal to sensor
 * - Reads 40 bits of data (humidity + temperature + checksum)
 * - Validates checksum
 * 
 * Note: DHT11 has ~1-2 second response time, don't poll faster than every 2 seconds
 * 
 * @param data Pointer to structure to store sensor data
 * @return esp_err_t ESP_OK if read was successful, error otherwise
 */
esp_err_t dht11_manager_read(dht11_data_t *data);

/**
 * @brief Get last valid sensor reading (cached)
 * 
 * Returns the last successful reading without performing a new sensor read.
 * Useful for avoiding frequent sensor polling.
 * 
 * @param data Pointer to structure to store cached sensor data
 * @return esp_err_t ESP_OK if cached data is available, ESP_ERR_INVALID_STATE if no valid data
 */
esp_err_t dht11_manager_get_cached(dht11_data_t *data);

#endif // DHT11_MANAGER_H
