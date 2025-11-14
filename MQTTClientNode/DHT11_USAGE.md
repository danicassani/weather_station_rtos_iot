# DHT11 Temperature & Humidity Sensor Module

## Overview

The DHT11 manager provides a simple interface to read temperature and humidity from the DHT11 digital sensor.

## Hardware Setup

### Wiring

DHT11 has 3 pins (or 4 pins with NC):

```
DHT11          ESP32
-----          -----
VCC   -------> 3.3V or 5V
DATA  -------> GPIO 4 (configurable via CONFIG_DHT11_GPIO)
GND   -------> GND
```

**Important**: DHT11 requires a pull-up resistor (4.7kΩ - 10kΩ) on the DATA line. Many DHT11 modules have this built-in.

## Configuration

Edit `main/include/config.h`:

```c
#define CONFIG_DHT11_GPIO         4    // GPIO pin connected to DHT11 data pin
#define CONFIG_DHT11_READ_INTERVAL 5000  // Minimum interval between reads (ms)
```

## API Usage

### Initialize Sensor

```c
#include "dht11_manager.h"

// Initialize on GPIO 4
esp_err_t err = dht11_manager_init(CONFIG_DHT11_GPIO);
if (err != ESP_OK) {
    ESP_LOGE(TAG, "DHT11 init failed");
}
```

### Read Sensor Data

```c
dht11_data_t data;
esp_err_t err = dht11_manager_read(&data);

if (err == ESP_OK && data.valid) {
    printf("Temperature: %.1f°C\n", data.temperature);
    printf("Humidity: %.1f%%\n", data.humidity);
} else {
    printf("Failed to read sensor\n");
}
```

### Get Cached Data

To avoid polling the sensor too frequently (which can cause errors), use cached data:

```c
dht11_data_t data;
esp_err_t err = dht11_manager_get_cached(&data);

if (err == ESP_OK && data.valid) {
    printf("Cached - Temp: %.1f°C, Humidity: %.1f%%\n", 
           data.temperature, data.humidity);
}
```

### Deinitialize

```c
dht11_manager_deinit();
```

## Data Structure

```c
typedef struct {
    float temperature;  // Temperature in Celsius
    float humidity;     // Relative humidity in %
    bool valid;         // True if data is valid
} dht11_data_t;
```

## Important Notes

1. **Timing Critical**: DHT11 uses a 1-wire protocol with microsecond-level timing. Interrupts are disabled during reads.

2. **Minimum Read Interval**: DHT11 requires at least 1-2 seconds between reads. Polling faster will cause read errors. Use `CONFIG_DHT11_READ_INTERVAL` to control this.

3. **Startup Time**: DHT11 needs ~1 second to stabilize after power-on. The `init` function includes this delay.

4. **Error Handling**: 
   - `ESP_ERR_TIMEOUT`: Sensor not responding (check wiring/power)
   - `ESP_ERR_INVALID_CRC`: Data corrupted (try again)
   - Use cached data as fallback

5. **Accuracy**: DHT11 specs:
   - Temperature: ±2°C (range: 0-50°C)
   - Humidity: ±5% RH (range: 20-90% RH)
   - Integer-only values (no decimals)

## Integration with MQTT

The main application automatically:
- Reads DHT11 every `CONFIG_DHT11_READ_INTERVAL` ms
- Publishes temperature and humidity in MQTT messages
- Uses cached data between reads to avoid sensor overload
- Handles failures gracefully (shows "N/A" if sensor unavailable)

Example MQTT message:
```
ESP32 | Client: ESP32_NODE_001 | IP: 192.168.1.100 | Temp: 23.0°C | Humidity: 65.0% | Time: 13-11-2025 10:30:45
```

## Troubleshooting

### Sensor Not Responding

1. Check wiring (especially pull-up resistor)
2. Verify GPIO number in config.h
3. Check power supply (3.3V or 5V depending on module)
4. Try a different GPIO pin

### Checksum Errors

1. Increase `CONFIG_DHT11_READ_INTERVAL` (DHT11 needs recovery time)
2. Check for electrical noise/interference
3. Ensure stable power supply
4. Try shorter wires (< 20cm recommended)

### Intermittent Reads

- Normal! DHT11 is not highly reliable
- Use cached data as fallback
- Consider averaging multiple readings
- For production, consider upgrading to DHT22 (better accuracy/reliability)

## Example Code

```c
// Read and publish sensor data
dht11_data_t sensor;
if (dht11_manager_read(&sensor) == ESP_OK && sensor.valid) {
    char msg[128];
    snprintf(msg, sizeof(msg), "Temp: %.1f°C, RH: %.1f%%",
             sensor.temperature, sensor.humidity);
    mqtt_manager_publish("sensors/dht11", msg, 0, 0);
}
```

## Log Configuration

Adjust DHT11 log level in `config.h`:

```c
#define CONFIG_LOG_LEVEL_DHT11    ESP_LOG_INFO   // INFO, DEBUG, VERBOSE, etc.
```

Use `ESP_LOG_DEBUG` to see detailed timing information during reads.
