#include "adc_scanner.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ADC_SCANNER";

// ADC handle
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;

// ADC1 GPIO to channel mapping for commonly available pins
typedef struct {
    int gpio;
    adc_channel_t channel;
} adc_gpio_map_t;

static const adc_gpio_map_t adc1_gpio_map[] = {
    {32, ADC_CHANNEL_4},
    {33, ADC_CHANNEL_5},
    {34, ADC_CHANNEL_6},
    {35, ADC_CHANNEL_7},
    {36, ADC_CHANNEL_0},
    {39, ADC_CHANNEL_3}
};

#define ADC1_GPIO_COUNT (sizeof(adc1_gpio_map) / sizeof(adc1_gpio_map[0]))

// Get channel for GPIO
static esp_err_t get_adc_channel(int gpio_num, adc_channel_t *channel)
{
    for (int i = 0; i < ADC1_GPIO_COUNT; i++) {
        if (adc1_gpio_map[i].gpio == gpio_num) {
            *channel = adc1_gpio_map[i].channel;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

// Initialize ADC calibration
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme: Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme: Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC calibration success");
    } else {
        ESP_LOGW(TAG, "ADC calibration failed, readings will be raw values");
    }
    return calibrated;
}

esp_err_t adc_scanner_init(void)
{
    if (adc1_handle != NULL) {
        ESP_LOGW(TAG, "ADC scanner already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Configure ADC1 unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC1 unit: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure all ADC1 channels
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,  // 12-bit
        .atten = ADC_ATTEN_DB_12,          // 0-3.3V range
    };

    for (int i = 0; i < ADC1_GPIO_COUNT; i++) {
        ret = adc_oneshot_config_channel(adc1_handle, adc1_gpio_map[i].channel, &config);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to configure ADC1 channel %d (GPIO %d): %s", 
                     adc1_gpio_map[i].channel, adc1_gpio_map[i].gpio, esp_err_to_name(ret));
        }
    }

    // Initialize calibration
    adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_12, &adc1_cali_handle);

    ESP_LOGI(TAG, "ADC scanner initialized (ADC1, 12-bit, 0-3.3V range)");
    return ESP_OK;
}

void adc_scanner_deinit(void)
{
    if (adc1_cali_handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(adc1_cali_handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_delete_scheme_line_fitting(adc1_cali_handle);
#endif
        adc1_cali_handle = NULL;
    }
    
    if (adc1_handle) {
        adc_oneshot_del_unit(adc1_handle);
        adc1_handle = NULL;
    }
    
    ESP_LOGI(TAG, "ADC scanner deinitialized");
}

esp_err_t adc_scanner_read_gpio(int gpio_num, int *raw_out, int *voltage_mv_out, int num_samples)
{
    if (!adc1_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    if (num_samples <= 0) {
        num_samples = 32;
    }

    adc_channel_t channel;
    esp_err_t ret = get_adc_channel(gpio_num, &channel);
    if (ret != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    // Read multiple samples and average
    int sum = 0;
    for (int i = 0; i < num_samples; i++) {
        int raw;
        ret = adc_oneshot_read(adc1_handle, channel, &raw);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ADC read failed on GPIO %d: %s", gpio_num, esp_err_to_name(ret));
            return ret;
        }
        sum += raw;
        vTaskDelay(pdMS_TO_TICKS(2));  // Small delay between samples
    }

    int raw_avg = sum / num_samples;
    if (raw_out) {
        *raw_out = raw_avg;
    }

    // Convert to voltage if calibration is available
    if (voltage_mv_out) {
        if (adc1_cali_handle) {
            int voltage;
            ret = adc_cali_raw_to_voltage(adc1_cali_handle, raw_avg, &voltage);
            if (ret == ESP_OK) {
                *voltage_mv_out = voltage;
            } else {
                // Fallback: linear approximation (3.3V = 4095 for 12-bit)
                *voltage_mv_out = (raw_avg * 3300) / 4095;
            }
        } else {
            // No calibration: linear approximation
            *voltage_mv_out = (raw_avg * 3300) / 4095;
        }
    }

    return ESP_OK;
}

esp_err_t adc_scanner_scan(adc_scan_result_t *results, size_t *num_results, size_t max_results)
{
    if (!adc1_handle) {
        ESP_LOGE(TAG, "ADC scanner not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!results || !num_results || max_results == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Scanning ADC1 channels for analog signals...");
    ESP_LOGI(TAG, "========================================");

    size_t count = 0;
    
    for (int i = 0; i < ADC1_GPIO_COUNT && count < max_results; i++) {
        int gpio = adc1_gpio_map[i].gpio;
        int raw = 0;
        int voltage_mv = 0;
        
        esp_err_t ret = adc_scanner_read_gpio(gpio, &raw, &voltage_mv, 64);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read GPIO %d", gpio);
            continue;
        }

        // Store result
        results[count].gpio_num = gpio;
        results[count].adc_channel = adc1_gpio_map[i].channel;
        results[count].raw_value = raw;
        results[count].voltage_mv = voltage_mv;
        
        // Heuristic: A connected analog sensor typically reads between 10% and 90% of range
        // (not stuck at 0V or 3.3V rail). For 12-bit ADC: ~400 to 3700
        bool looks_analog = (raw > 400 && raw < 3700);
        results[count].looks_connected = looks_analog;
        
        ESP_LOGI(TAG, "GPIO %2d (CH%d): Raw=%4d, Voltage=%4d mV %s", 
                 gpio, 
                 adc1_gpio_map[i].channel,
                 raw, 
                 voltage_mv,
                 looks_analog ? "✓ [ANALOG SIGNAL DETECTED]" : "");
        
        count++;
    }

    *num_results = count;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Scan complete. %d channels tested.", count);
    
    // Summary: list candidates
    int candidates = 0;
    for (int i = 0; i < count; i++) {
        if (results[i].looks_connected) {
            candidates++;
        }
    }
    
    if (candidates > 0) {
        ESP_LOGI(TAG, "Found %d potential analog sensor(s):", candidates);
        for (int i = 0; i < count; i++) {
            if (results[i].looks_connected) {
                ESP_LOGI(TAG, "  → GPIO %d: %d mV", results[i].gpio_num, results[i].voltage_mv);
            }
        }
    } else {
        ESP_LOGW(TAG, "No analog signals detected. Possible causes:");
        ESP_LOGW(TAG, "  1) Sensor not powered or not connected");
        ESP_LOGW(TAG, "  2) Sensor on ADC2 (incompatible with WiFi)");
        ESP_LOGW(TAG, "  3) Sensor outputs digital signal, not analog");
    }
    ESP_LOGI(TAG, "========================================");

    return ESP_OK;
}
