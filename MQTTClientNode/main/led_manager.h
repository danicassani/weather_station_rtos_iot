#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Inicializa el LED (sin timer automático)
 * 
 * @param gpio_num Número del pin GPIO para el LED
 * @return esp_err_t ESP_OK si fue exitoso
 */
esp_err_t led_manager_init(int gpio_num);

/**
 * @brief Desinicializa el LED
 * 
 * @return esp_err_t ESP_OK si fue exitoso
 */
esp_err_t led_manager_deinit(void);

/**
 * @brief Establece el estado del LED
 * 
 * @param level true para encender, false para apagar
 * @return esp_err_t ESP_OK si fue exitoso
 */
esp_err_t led_manager_set_level(bool level);

/**
 * @brief Enciende el LED por un período de tiempo específico y luego lo apaga
 * 
 * @param duration_ms Duración en milisegundos que el LED permanecerá encendido
 * @return esp_err_t ESP_OK si fue exitoso
 */
esp_err_t led_manager_pulse(uint32_t duration_ms);

#endif // LED_MANAGER_H
