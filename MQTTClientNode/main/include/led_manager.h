#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initialize the LED (without automatic timer)
 * 
 * @param gpio_num GPIO pin number for the LED
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t led_manager_init(int gpio_num);

/**
 * @brief Deinitialize the LED
 * 
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t led_manager_deinit(void);

/**
 * @brief Set the LED state
 * 
 * @param level true to turn on, false to turn off
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t led_manager_set_level(bool level);

/**
 * @brief Turn on the LED for a specific period of time and then turn it off
 * 
 * @param duration_ms Duration in milliseconds that the LED will remain on
 * @return esp_err_t ESP_OK if successful
 */
esp_err_t led_manager_pulse(uint32_t duration_ms);

#endif // LED_MANAGER_H
