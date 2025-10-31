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
#define CONFIG_MQTT_BROKER_URI    "mqtt://192.168.1.250"
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
// Application Configuration
// ============================================================================
#define CONFIG_PUBLISH_INTERVAL   10000  // MQTT publish interval in milliseconds
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
#define CONFIG_APP_LOG_LEVEL      ESP_LOG_INFO

// Per-module log levels (override global level for specific components)
#define CONFIG_LOG_LEVEL_WIFI     ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_MQTT     ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_LED      ESP_LOG_WARN   // Less verbose for LED
#define CONFIG_LOG_LEVEL_TELNET   ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_INIT     ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_MAIN     ESP_LOG_INFO

#endif // CONFIG_H
