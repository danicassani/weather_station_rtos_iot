#include "dht11_manager.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include <string.h>

static const char *TAG = "DHT11_MANAGER";

// DHT11 timing constants (microseconds)
#define DHT11_START_SIGNAL_LOW_TIME   20000  // 18-20ms (increased for reliability)
#define DHT11_START_SIGNAL_HIGH_TIME  30     // 20-40us
#define DHT11_RESPONSE_TIMEOUT        200    // Max wait for response (increased)
#define DHT11_BIT_TIMEOUT             150    // Max wait for bit signal (increased)

// Module state
static struct {
    int gpio_num;
    bool initialized;
    dht11_data_t last_reading;
} dht11_state = {
    .gpio_num = -1,
    .initialized = false,
    .last_reading = {0}
};

/**
 * @brief Read 40 bits of data from DHT11 (must be called from critical section)
 * 
 * @param data Buffer to store 5 bytes (humidity_int, humidity_dec, temp_int, temp_dec, checksum)
 * @return esp_err_t ESP_OK if successful
 * 
 * NOTE: This function MUST be called with interrupts disabled.
 * NO logging allowed inside this function!
 */
static esp_err_t dht11_read_raw(uint8_t data[5])
{
    memset(data, 0, 5);
    int timeout;
    
    // Send start signal: LOW for 18-20ms, then HIGH for 20-40us
    gpio_set_direction(dht11_state.gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_state.gpio_num, 0);
    ets_delay_us(DHT11_START_SIGNAL_LOW_TIME);
    
    gpio_set_level(dht11_state.gpio_num, 1);
    ets_delay_us(DHT11_START_SIGNAL_HIGH_TIME);
    
    // Switch to input mode to read sensor response
    gpio_set_direction(dht11_state.gpio_num, GPIO_MODE_INPUT);
    
    // Small delay to let the line stabilize
    ets_delay_us(2);
    
    // Wait for sensor to pull line LOW (response signal ~80us)
    timeout = 0;
    while (gpio_get_level(dht11_state.gpio_num) == 1) {
        if (++timeout > DHT11_RESPONSE_TIMEOUT) {
            return ESP_ERR_TIMEOUT;  // Error code 1: no response LOW
        }
        ets_delay_us(1);
    }
    
    // Wait for sensor to pull line HIGH (~80us)
    timeout = 0;
    while (gpio_get_level(dht11_state.gpio_num) == 0) {
        if (++timeout > DHT11_RESPONSE_TIMEOUT) {
            return ESP_ERR_TIMEOUT;  // Error code 2: no response HIGH
        }
        ets_delay_us(1);
    }
    
    // Wait for sensor to pull line LOW (start of data transmission ~50us)
    timeout = 0;
    while (gpio_get_level(dht11_state.gpio_num) == 1) {
        if (++timeout > DHT11_RESPONSE_TIMEOUT) {
            return ESP_ERR_TIMEOUT;  // Error code 3: no data start
        }
        ets_delay_us(1);
    }
    
    // Read 40 bits of data
    for (int i = 0; i < 40; i++) {
        // Wait for LOW to HIGH transition (start of bit ~50us LOW)
        timeout = 0;
        while (gpio_get_level(dht11_state.gpio_num) == 0) {
            if (++timeout > DHT11_BIT_TIMEOUT) {
                return ESP_ERR_TIMEOUT;  // Timeout on bit start
            }
            ets_delay_us(1);
        }
        
        // Measure HIGH pulse duration to determine bit value
        // 26-28us = 0, 70us = 1
        ets_delay_us(40);  // Sample in the middle
        int bit_value = gpio_get_level(dht11_state.gpio_num);
        
        // Store bit
        int byte_idx = i / 8;
        int bit_idx = 7 - (i % 8);  // MSB first
        if (bit_value) {
            data[byte_idx] |= (1 << bit_idx);
        }
        
        // Wait for bit to end (line goes LOW)
        timeout = 0;
        while (gpio_get_level(dht11_state.gpio_num) == 1) {
            if (++timeout > DHT11_BIT_TIMEOUT) {
                return ESP_ERR_TIMEOUT;  // Timeout waiting for bit end
            }
            ets_delay_us(1);
        }
    }
    
    return ESP_OK;
}

/**
 * @brief Validate checksum and parse sensor data
 * 
 * @param raw_data Raw 5-byte data from sensor
 * @param parsed Output parsed data structure
 * @return esp_err_t ESP_OK if checksum valid
 */
static esp_err_t dht11_parse_data(const uint8_t raw_data[5], dht11_data_t *parsed)
{
    // DHT11 data format:
    // [0] = Humidity integer part
    // [1] = Humidity decimal part (always 0 for DHT11)
    // [2] = Temperature integer part
    // [3] = Temperature decimal part (always 0 for DHT11)
    // [4] = Checksum (sum of bytes 0-3)
    
    uint8_t checksum = raw_data[0] + raw_data[1] + raw_data[2] + raw_data[3];
    if (checksum != raw_data[4]) {
        ESP_LOGE(TAG, "Checksum error: calculated=0x%02X, received=0x%02X", 
                 checksum, raw_data[4]);
        return ESP_ERR_INVALID_CRC;
    }
    
    // DHT11 only provides integer values, decimal parts are always 0
    parsed->humidity = (float)raw_data[0];
    parsed->temperature = (float)raw_data[2];
    parsed->valid = true;
    
    ESP_LOGD(TAG, "Sensor data: Temp=%.1f°C, Humidity=%.1f%%", 
             parsed->temperature, parsed->humidity);
    
    return ESP_OK;
}

esp_err_t dht11_manager_init(int gpio_num)
{
    if (gpio_num < 0 || gpio_num > 39) {
        ESP_LOGE(TAG, "Invalid GPIO number: %d", gpio_num);
        return ESP_ERR_INVALID_ARG;
    }

    if (dht11_state.initialized) {
        ESP_LOGW(TAG, "DHT11 manager already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Configure GPIO with pull-up (DHT11 requires pull-up resistor)
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio_num,
        .mode = GPIO_MODE_OUTPUT_OD,  // Open-drain for bidirectional communication
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", gpio_num, esp_err_to_name(err));
        return err;
    }

    // Set initial state to HIGH
    gpio_set_level(gpio_num, 1);

    dht11_state.gpio_num = gpio_num;
    dht11_state.initialized = true;
    dht11_state.last_reading.valid = false;

    ESP_LOGI(TAG, "DHT11 manager initialized on GPIO %d", gpio_num);
    ESP_LOGI(TAG, "Waiting 1s for sensor stabilization...");

    // Wait for sensor stabilization (DHT11 needs ~1s after power-on)
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "DHT11 ready. Ensure sensor has 4.7k-10k pull-up resistor on data line");

    return ESP_OK;
}

esp_err_t dht11_manager_deinit(void)
{
    if (!dht11_state.initialized) {
        ESP_LOGW(TAG, "DHT11 manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Reset GPIO
    if (dht11_state.gpio_num >= 0) {
        gpio_reset_pin(dht11_state.gpio_num);
        dht11_state.gpio_num = -1;
    }
    
    dht11_state.initialized = false;
    dht11_state.last_reading.valid = false;
    
    ESP_LOGI(TAG, "DHT11 manager deinitialized");
    return ESP_OK;
}

esp_err_t dht11_manager_read(dht11_data_t *data)
{
    if (!dht11_state.initialized) {
        ESP_LOGE(TAG, "DHT11 manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (data == NULL) {
        ESP_LOGE(TAG, "Data pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t raw_data[5];
    esp_err_t err;
    
    // Disable interrupts during timing-critical read operation
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);
    
    err = dht11_read_raw(raw_data);
    
    portEXIT_CRITICAL(&mux);
    
    // NOW it's safe to log - interrupts are re-enabled
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read sensor data (timeout during protocol handshake)");
        ESP_LOGE(TAG, "Troubleshooting: Check GPIO %d wiring, pull-up resistor (4.7k-10k), and sensor power", 
                 dht11_state.gpio_num);
        data->valid = false;
        return err;
    }
    
    // Parse and validate data
    err = dht11_parse_data(raw_data, data);
    if (err != ESP_OK) {
        data->valid = false;
        return err;
    }
    
    // Cache last valid reading
    memcpy(&dht11_state.last_reading, data, sizeof(dht11_data_t));
    
    ESP_LOGD(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", 
             data->temperature, data->humidity);
    
    return ESP_OK;
}

esp_err_t dht11_manager_get_cached(dht11_data_t *data)
{
    if (!dht11_state.initialized) {
        ESP_LOGE(TAG, "DHT11 manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (data == NULL) {
        ESP_LOGE(TAG, "Data pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!dht11_state.last_reading.valid) {
        ESP_LOGW(TAG, "No valid cached data available");
        return ESP_ERR_INVALID_STATE;
    }
    
    memcpy(data, &dht11_state.last_reading, sizeof(dht11_data_t));
    return ESP_OK;
}
