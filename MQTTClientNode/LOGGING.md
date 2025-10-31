# Sistema de Logging Configurable

## 📝 Descripción

El proyecto implementa un sistema de logging jerárquico basado en ESP-IDF que permite configurar niveles de log globales y por módulo.

## 🎚️ Niveles de Log Disponibles

Los niveles están definidos por ESP-IDF en orden de verbosidad:

| Nivel | Valor | Descripción | Uso |
|-------|-------|-------------|-----|
| `ESP_LOG_NONE` | 0 | Sin logs | Producción optimizada |
| `ESP_LOG_ERROR` | 1 | Solo errores críticos | Producción normal |
| `ESP_LOG_WARN` | 2 | Advertencias + Errores | Producción con monitoreo |
| `ESP_LOG_INFO` | 3 | Información general | Desarrollo normal |
| `ESP_LOG_DEBUG` | 4 | Debugging detallado | Debug activo |
| `ESP_LOG_VERBOSE` | 5 | Máximo detalle | Debug profundo |

## ⚙️ Configuración en `config.h`

### Nivel Global

```c
// Cambia el nivel global de todos los módulos
#define CONFIG_APP_LOG_LEVEL      ESP_LOG_INFO
```

### Niveles por Módulo

```c
// Configura cada módulo individualmente
#define CONFIG_LOG_LEVEL_WIFI     ESP_LOG_INFO    // WiFi: normal
#define CONFIG_LOG_LEVEL_MQTT     ESP_LOG_DEBUG   // MQTT: debug activo
#define CONFIG_LOG_LEVEL_LED      ESP_LOG_WARN    // LED: solo warnings
#define CONFIG_LOG_LEVEL_TELNET   ESP_LOG_INFO    // Telnet: normal
#define CONFIG_LOG_LEVEL_INIT     ESP_LOG_INFO    // Init: normal
#define CONFIG_LOG_LEVEL_MAIN     ESP_LOG_DEBUG   // Main: debug activo
```

## 📊 Ejemplos de Uso

### Configuración para Producción (mínimo output)
```c
#define CONFIG_APP_LOG_LEVEL      ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_WIFI     ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_MQTT     ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_LED      ESP_LOG_NONE    // LED silencioso
#define CONFIG_LOG_LEVEL_TELNET   ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_INIT     ESP_LOG_INFO    // Ver arranque
#define CONFIG_LOG_LEVEL_MAIN     ESP_LOG_WARN
```

### Configuración para Debug MQTT
```c
#define CONFIG_APP_LOG_LEVEL      ESP_LOG_INFO
#define CONFIG_LOG_LEVEL_WIFI     ESP_LOG_INFO
#define CONFIG_LOG_LEVEL_MQTT     ESP_LOG_VERBOSE  // Máximo detalle MQTT
#define CONFIG_LOG_LEVEL_LED      ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_TELNET   ESP_LOG_INFO
#define CONFIG_LOG_LEVEL_INIT     ESP_LOG_INFO
#define CONFIG_LOG_LEVEL_MAIN     ESP_LOG_DEBUG
```

### Configuración para Debug WiFi
```c
#define CONFIG_APP_LOG_LEVEL      ESP_LOG_INFO
#define CONFIG_LOG_LEVEL_WIFI     ESP_LOG_DEBUG    // Debug WiFi activo
#define CONFIG_LOG_LEVEL_MQTT     ESP_LOG_INFO
#define CONFIG_LOG_LEVEL_LED      ESP_LOG_WARN
#define CONFIG_LOG_LEVEL_TELNET   ESP_LOG_INFO
#define CONFIG_LOG_LEVEL_INIT     ESP_LOG_INFO
#define CONFIG_LOG_LEVEL_MAIN     ESP_LOG_INFO
```

## 🔍 Cómo Usar en el Código

### Macros Disponibles

```c
ESP_LOGE(TAG, "Error crítico: %s", error_msg);      // Rojo - Siempre importante
ESP_LOGW(TAG, "Advertencia: reintentos=%d", n);     // Amarillo - Atención
ESP_LOGI(TAG, "Conectado exitosamente");            // Verde - Info general
ESP_LOGD(TAG, "Debug: buffer_size=%d", size);       // - Solo en debug
ESP_LOGV(TAG, "Verbose: dump completo...");         // - Máximo detalle
```

### Ejemplo Práctico en main.c

```c
ESP_LOGD(TAG, "MQTT connected, preparing to publish...");  // Solo si CONFIG_LOG_LEVEL_MAIN >= DEBUG
ESP_LOGI(TAG, "Publicando mensaje en topic '%s'...", topic); // Siempre visible con INFO
ESP_LOGD(TAG, "IP: %s, Timestamp: %s", ip, ts);            // Solo en debug
ESP_LOGE(TAG, "Error al publicar mensaje");                // Siempre visible (error)
```

## 🚀 Ventajas

- ✅ **Control granular**: Cada módulo independiente
- ✅ **Fácil configuración**: Un solo archivo (`config.h`)
- ✅ **Sin recompilar todo**: Solo cambia defines
- ✅ **Optimización**: Logs de debug se eliminan en compilación si no se usan
- ✅ **Compatible con Telnet**: Los niveles afectan también a logs remotos

## 📌 Notas Importantes

1. Los logs **se evalúan en tiempo de compilación** cuando es posible
2. Cambiar `CONFIG_APP_LOG_LEVEL` requiere **recompilar** (`idf.py build`)
3. Los niveles por módulo **sobrescriben** el nivel global
4. TAG debe coincidir con el nombre usado en `configure_log_levels()`

## 🛠️ Añadir Nuevo Módulo

1. Define el nivel en `config.h`:
   ```c
   #define CONFIG_LOG_LEVEL_MI_MODULO    ESP_LOG_INFO
   ```

2. Añade en `system_init.c` → `configure_log_levels()`:
   ```c
   esp_log_level_set("MI_MODULO", CONFIG_LOG_LEVEL_MI_MODULO);
   ```

3. Usa en tu módulo:
   ```c
   static const char *TAG = "MI_MODULO";
   ESP_LOGI(TAG, "Mi mensaje");
   ```

¡Listo! 🎉
