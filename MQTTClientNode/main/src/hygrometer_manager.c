#include "hygrometer_manager.h"
#include "adc_scanner.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "HYGROMETER_MANAGER";

// Manager state
typedef struct {
    bool initialized;
    int gpio_num;
    int dry_value;   // ADC value when dry (air) - typically higher voltage
    int wet_value;   // ADC value when wet (water) - typically lower voltage
    hygrometer_data_t last_reading;
} hygrometer_state_t;

static hygrometer_state_t hygro_state = {
    .initialized = false,
    .gpio_num = -1,
    .dry_value = CONFIG_HYGROMETER_DRY_VALUE,
    .wet_value = CONFIG_HYGROMETER_WET_VALUE,
    .last_reading = {0}
};

esp_err_t hygrometer_manager_init(int gpio_num)
{
    if (gpio_num < 0 || gpio_num > 39) {
        ESP_LOGE(TAG, "Invalid GPIO number: %d", gpio_num);
        return ESP_ERR_INVALID_ARG;
    }

    if (hygro_state.initialized) {
        ESP_LOGW(TAG, "Hygrometer manager already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Verify GPIO is an ADC1 channel (compatible with WiFi)
    // ADC1: GPIO 32-39
    if (gpio_num < 32 || gpio_num > 39) {
        ESP_LOGE(TAG, "GPIO %d is not an ADC1 channel. Use GPIO 32-39 only.", gpio_num);
        ESP_LOGE(TAG, "ADC2 channels cannot be used while WiFi is active.");
        return ESP_ERR_INVALID_ARG;
    }

    hygro_state.gpio_num = gpio_num;
    hygro_state.initialized = true;
    hygro_state.last_reading.valid = false;

    ESP_LOGI(TAG, "Hygrometer manager initialized on GPIO %d", gpio_num);
    ESP_LOGI(TAG, "Calibration: Dry=%d (0%%), Wet=%d (100%%)", 
             hygro_state.dry_value, hygro_state.wet_value);
    
    // Perform initial reading
    hygrometer_data_t initial_data;
    esp_err_t ret = hygrometer_manager_read(&initial_data);
    if (ret == ESP_OK && initial_data.valid) {
        ESP_LOGI(TAG, "Initial reading: %d mV (%.1f%% moisture)", 
                 initial_data.voltage_mv, initial_data.moisture_percent);
    } else {
        ESP_LOGW(TAG, "Initial reading failed, sensor may not be connected");
    }

    return ESP_OK;
}

void hygrometer_manager_deinit(void)
{
    if (hygro_state.initialized) {
        hygro_state.initialized = false;
        hygro_state.gpio_num = -1;
        ESP_LOGI(TAG, "Hygrometer manager deinitialized");
    }
}

esp_err_t hygrometer_manager_read(hygrometer_data_t *data)
{
    if (!hygro_state.initialized) {
        ESP_LOGE(TAG, "Hygrometer manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    // Read ADC (multi-sample average)
    int raw_value = 0;
    int voltage_mv = 0;
    esp_err_t ret = adc_scanner_read_gpio(hygro_state.gpio_num, &raw_value, &voltage_mv, 
                                          CONFIG_HYGROMETER_NUM_SAMPLES);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC on GPIO %d: %s", 
                 hygro_state.gpio_num, esp_err_to_name(ret));
        data->valid = false;
        return ret;
    }

    // Store raw values
    data->raw_value = raw_value;
    data->voltage_mv = voltage_mv;
    data->valid = true;

    // Calculate moisture percentage using calibration
    // Most soil moisture sensors read higher voltage when dry, lower when wet
    // So: 0% moisture = dry_value, 100% moisture = wet_value
    
    int range = hygro_state.dry_value - hygro_state.wet_value;
    if (range <= 0) {
        ESP_LOGW(TAG, "Invalid calibration: dry_value must be > wet_value");
        data->moisture_percent = 0.0f;
    } else {
        // Map: dry_value -> 0%, wet_value -> 100%
        float percent = ((float)(hygro_state.dry_value - raw_value) / (float)range) * 100.0f;
        
        // Clamp to 0-100%
        if (percent < 0.0f) percent = 0.0f;
        if (percent > 100.0f) percent = 100.0f;
        
        data->moisture_percent = percent;
    }

    // Cache the reading
    memcpy(&hygro_state.last_reading, data, sizeof(hygrometer_data_t));

    ESP_LOGD(TAG, "Hygrometer read: Raw=%d, Voltage=%d mV, Moisture=%.1f%%", 
             raw_value, voltage_mv, data->moisture_percent);

    return ESP_OK;
}

esp_err_t hygrometer_manager_get_cached(hygrometer_data_t *data)
{
    if (!hygro_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!hygro_state.last_reading.valid) {
        ESP_LOGD(TAG, "No valid cached data available");
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(data, &hygro_state.last_reading, sizeof(hygrometer_data_t));
    return ESP_OK;
}

esp_err_t hygrometer_manager_set_calibration(int dry_value, int wet_value)
{
    if (!hygro_state.initialized) {
        ESP_LOGE(TAG, "Hygrometer manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (dry_value <= wet_value) {
        ESP_LOGE(TAG, "Invalid calibration: dry_value (%d) must be > wet_value (%d)", 
                 dry_value, wet_value);
        return ESP_ERR_INVALID_ARG;
    }

    hygro_state.dry_value = dry_value;
    hygro_state.wet_value = wet_value;

    ESP_LOGI(TAG, "Calibration updated: Dry=%d (0%%), Wet=%d (100%%)", 
             dry_value, wet_value);

    return ESP_OK;
}
