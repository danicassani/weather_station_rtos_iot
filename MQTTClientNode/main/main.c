#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "BLINK";

// Valores desde menuconfig (Kconfig.projbuild)
#ifndef CONFIG_BLINK_GPIO
#define CONFIG_BLINK_GPIO 2
#endif

#ifndef CONFIG_BLINK_PERIOD_MS
#define CONFIG_BLINK_PERIOD_MS 50
#endif

static void configure_led_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << CONFIG_BLINK_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config falló: %s", esp_err_to_name(err));
    } else {
        // Arrancamos en LOW para no deslumbrar
        gpio_set_level(CONFIG_BLINK_GPIO, 0);
        ESP_LOGI(TAG, "GPIO %d configurado como salida", CONFIG_BLINK_GPIO);
    }
}

static void blink_task(void *pv)
{
    const TickType_t half_period = pdMS_TO_TICKS(CONFIG_BLINK_PERIOD_MS / 2);
    bool level = false;

    ESP_LOGI(TAG, "Parpadeo: periodo=%d ms (GPIO=%d)", CONFIG_BLINK_PERIOD_MS, CONFIG_BLINK_GPIO);

    while (true) {
        level = !level;
        gpio_set_level(CONFIG_BLINK_GPIO, level);
        vTaskDelay(half_period);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Arrancando blink…");
    configure_led_gpio();

    BaseType_t ok = xTaskCreatePinnedToCore(
        blink_task,        // función
        "blink_task",      // nombre
        2048,              // stack
        NULL,              // parámetro
        5,                 // prioridad
        NULL,              // handle
        tskNO_AFFINITY     // core (o 0/1 según chip)
    );

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "No pude crear blink_task (me he quedao sin RAM del stack).");
    }
}
