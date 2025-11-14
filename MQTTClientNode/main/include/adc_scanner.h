#ifndef ADC_SCANNER_H
#define ADC_SCANNER_H

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

/**
 * @brief ADC scan result for a single GPIO
 */
typedef struct {
    int gpio_num;           // GPIO number
    int adc_channel;        // ADC channel number
    int raw_value;          // Raw ADC reading (0-4095 for 12-bit)
    int voltage_mv;         // Voltage in millivolts (0-3300mV typically)
    bool looks_connected;   // True if this pin shows analog signal (not rail voltage)
} adc_scan_result_t;

/**
 * @brief Initialize ADC scanner (configures ADC1 unit)
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t adc_scanner_init(void);

/**
 * @brief Deinitialize ADC scanner and free resources
 */
void adc_scanner_deinit(void);

/**
 * @brief Scan all ADC1 channels and report readings
 * 
 * Reads each ADC1 GPIO multiple times, averages the values,
 * and flags channels that appear to have analog sensors connected
 * (i.e., not stuck at 0V or 3.3V rail).
 * 
 * @param results Array to store results (must have space for at least 6 entries)
 * @param num_results Pointer to store number of results written
 * @param max_results Maximum number of results to write
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t adc_scanner_scan(adc_scan_result_t *results, size_t *num_results, size_t max_results);

/**
 * @brief Read a specific ADC1 channel multiple times and average
 * 
 * @param gpio_num GPIO number to read (must be ADC1 channel)
 * @param raw_out Pointer to store averaged raw value
 * @param voltage_mv_out Pointer to store voltage in mV
 * @param num_samples Number of samples to average (default 32)
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t adc_scanner_read_gpio(int gpio_num, int *raw_out, int *voltage_mv_out, int num_samples);

#endif // ADC_SCANNER_H
