#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include "esp_err.h"

/**
 * @brief Read all sensors and publish data via MQTT
 * 
 * Reads DHT11 and hygrometer sensors (respecting minimum intervals),
 * builds a JSON payload with all sensor data and timestamp,
 * and publishes to the configured MQTT topic.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mqtt_publish_sensor_data(void);

#endif // MQTT_PUBLISHER_H
