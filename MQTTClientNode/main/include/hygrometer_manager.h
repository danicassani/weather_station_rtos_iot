#ifndef HYGROMETER_MANAGER_H
#define HYGROMETER_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Hygrometer sensor data
 */
typedef struct {
    int raw_value;          // Raw ADC reading (0-4095 for 12-bit)
    int voltage_mv;         // Voltage in millivolts
    float moisture_percent; // Soil moisture percentage (0-100%)
    bool valid;             // True if reading is valid
} hygrometer_data_t;

/**
 * @brief Initialize hygrometer manager
 * 
 * Configures the ADC channel for the specified GPIO and sets up calibration.
 * 
 * @param gpio_num GPIO number connected to hygrometer sensor (must be ADC1 channel)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t hygrometer_manager_init(int gpio_num);

/**
 * @brief Deinitialize hygrometer manager and free resources
 */
void hygrometer_manager_deinit(void);

/**
 * @brief Read current hygrometer value
 * 
 * Reads the ADC, averages multiple samples, and converts to percentage
 * using the configured calibration values (dry/wet thresholds).
 * 
 * @param data Pointer to store reading result
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t hygrometer_manager_read(hygrometer_data_t *data);

/**
 * @brief Get cached hygrometer reading
 * 
 * Returns the last successful reading without performing a new ADC read.
 * 
 * @param data Pointer to store cached reading
 * @return ESP_OK if cached data is valid, ESP_ERR_INVALID_STATE otherwise
 */
esp_err_t hygrometer_manager_get_cached(hygrometer_data_t *data);

/**
 * @brief Set calibration values for moisture percentage calculation
 * 
 * The sensor typically reads higher voltage when dry (air) and lower when wet (water).
 * These values map raw ADC readings to 0-100% moisture.
 * 
 * @param dry_value Raw ADC value when sensor is completely dry (0% moisture)
 * @param wet_value Raw ADC value when sensor is completely wet (100% moisture)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t hygrometer_manager_set_calibration(int dry_value, int wet_value);

#endif // HYGROMETER_MANAGER_H
