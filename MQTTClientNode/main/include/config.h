#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// ============================================================================
// WiFi Configuration
// ============================================================================
#define CONFIG_WIFI_SSID      "DIGIFIBRA-2xt5"
#define CONFIG_WIFI_PASSWORD  "uTDcSbN74edQ"
#define CONFIG_WIFI_MAX_RETRY 10

// ============================================================================
// MQTT Configuration
// ============================================================================
#define CONFIG_MQTT_BROKER_URI    "mqtt://192.168.1.135"
#define CONFIG_MQTT_CLIENT_ID     "ESP32_NODE_001"
#define CONFIG_MQTT_TOPIC         "ESP32"
#define CONFIG_MQTT_LWT_TOPIC     "disconnections"
#define CONFIG_MQTT_KEEPALIVE     60  // seconds
#define CONFIG_MQTT_QOS           1   // 0, 1, or 2

// ============================================================================
// Telnet Logger Configuration
// ============================================================================
#define CONFIG_TELNET_PORT        23
#define CONFIG_TELNET_ENABLED     1   // Set to 0 to disable Telnet logging

// ============================================================================
// NTP/Time Configuration
// ============================================================================
#define CONFIG_NTP_SERVER         "pool.ntp.org"
#define CONFIG_TIMEZONE           "CET-1CEST,M3.5.0,M10.5.0/3"  // Central Europe
#define CONFIG_NTP_SYNC_TIMEOUT   10  // Max retries for NTP sync

// ============================================================================
// LED Configuration
// ============================================================================
#ifndef CONFIG_BLINK_GPIO
#define CONFIG_BLINK_GPIO         2   // Default GPIO for LED (can be overridden by menuconfig)
#endif

#define CONFIG_LED_PULSE_MS       500  // LED pulse duration in milliseconds

// ============================================================================
// DHT11 Sensor Configuration
// ============================================================================
#define CONFIG_DHT11_GPIO         22   // GPIO pin connected to DHT11 data pin (ignored if AUTO_SCAN enabled)
#define CONFIG_DHT11_READ_INTERVAL 1000  // Minimum interval between reads (ms), DHT11 needs 2s min
#define CONFIG_DHT11_AUTO_SCAN    0    // Set to 1 to auto-detect DHT11 GPIO, 0 to use CONFIG_DHT11_GPIO

// ============================================================================
// ADC / Hygrometer Configuration
// ============================================================================
// Note: ADC2 (GPIO 0,2,4,12-15,25-27) cannot be used with WiFi enabled
// Use ADC1 channels only: GPIO 32-39 (commonly 32,33,34,35,36,39)
#define CONFIG_HYGROMETER_GPIO         32    // GPIO connected to hygrometer sensor (ADC1_CH4)
#define CONFIG_HYGROMETER_READ_INTERVAL 1000 // Minimum interval between reads (ms)
#define CONFIG_HYGROMETER_NUM_SAMPLES  64    // Number of ADC samples to average per reading

// Calibration values: map ADC raw values to moisture percentage
// Typical behavior: sensor reads higher voltage when dry, lower when wet
// To calibrate: 
//   1) Place sensor in dry air, note the raw value -> set as DRY_VALUE (0% moisture)
//   2) Place sensor in water, note the raw value -> set as WET_VALUE (100% moisture)
#define CONFIG_HYGROMETER_DRY_VALUE    2850  // Raw ADC value when completely dry (0% moisture)
#define CONFIG_HYGROMETER_WET_VALUE    1550  // Raw ADC value when completely wet (100% moisture)

// ============================================================================
// Application Configuration
// ============================================================================
#define CONFIG_PUBLISH_INTERVAL   1000  // MQTT publish interval in milliseconds
#define CONFIG_STARTUP_DELAY      2000   // Delay after init before starting main loop

// ============================================================================
// Logging Configuration
// ============================================================================
// Log levels (based on ESP-IDF log levels):
// ESP_LOG_NONE       (0) - No log output
// ESP_LOG_ERROR      (1) - Critical errors, software module cannot recover on its own
// ESP_LOG_WARN       (2) - Error conditions from which recovery measures have been taken
// ESP_LOG_INFO       (3) - Information messages which describe normal flow of events
// ESP_LOG_DEBUG      (4) - Extra information which is not necessary for normal use (values, pointers, sizes, etc)
// ESP_LOG_VERBOSE    (5) - Bigger chunks of debugging information, or frequent messages

// Set global log level (default: ESP_LOG_INFO)
// Change this to ESP_LOG_DEBUG for more verbose output, or ESP_LOG_WARN for less
#define CONFIG_APP_LOG_LEVEL      ESP_LOG_DEBUG

// Per-module log levels (override global level for specific components)
#define CONFIG_LOG_LEVEL_WIFI     ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_MQTT     ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_LED      ESP_LOG_WARN   // Less verbose for LED
#define CONFIG_LOG_LEVEL_TELNET   ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_INIT     ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_MAIN     ESP_LOG_INFO
#define CONFIG_LOG_LEVEL_DHT11    ESP_LOG_DEBUG   // DHT11 sensor logging
#define CONFIG_LOG_LEVEL_ADC      ESP_LOG_DEBUG   // ADC scanner logging
#define CONFIG_LOG_LEVEL_HYGRO    ESP_LOG_DEBUG   // Hygrometer logging

#endif // CONFIG_H
