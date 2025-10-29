#include "led_manager.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_MANAGER";

static int s_gpio_num = -1;
static gptimer_handle_t s_pulse_timer = NULL;

// Callback del timer para apagar el LED después del pulso
static bool IRAM_ATTR pulse_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    gpio_set_level(s_gpio_num, 0);  // Apagar LED
    gptimer_stop(timer);
    return false;
}

esp_err_t led_manager_init(int gpio_num)
{
    if (gpio_num < 0) {
        ESP_LOGE(TAG, "Número de GPIO inválido: %d", gpio_num);
        return ESP_ERR_INVALID_ARG;
    }

    s_gpio_num = gpio_num;

    // Configurar GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio_num,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al configurar GPIO %d: %s", gpio_num, esp_err_to_name(err));
        return err;
    }

    // Iniciar en LOW
    gpio_set_level(gpio_num, 0);
    ESP_LOGI(TAG, "GPIO %d configurado como salida", gpio_num);

    // Configurar timer one-shot para pulsos
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1 MHz, tick = 1 µs
    };
    
    err = gptimer_new_timer(&timer_config, &s_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al crear timer: %s", esp_err_to_name(err));
        return err;
    }

    // Configurar callback
    gptimer_event_callbacks_t cbs = {
        .on_alarm = pulse_timer_callback,
    };
    err = gptimer_register_event_callbacks(s_pulse_timer, &cbs, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al registrar callback: %s", esp_err_to_name(err));
        gptimer_del_timer(s_pulse_timer);
        return err;
    }

    err = gptimer_enable(s_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al habilitar timer: %s", esp_err_to_name(err));
        gptimer_del_timer(s_pulse_timer);
        return err;
    }

    ESP_LOGI(TAG, "LED manager iniciado: GPIO=%d", gpio_num);
    return ESP_OK;
}

esp_err_t led_manager_deinit(void)
{
    if (s_pulse_timer == NULL) {
        ESP_LOGW(TAG, "LED manager no estaba inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;

    // Detener timer si está corriendo
    gptimer_stop(s_pulse_timer);

    // Deshabilitar timer
    err = gptimer_disable(s_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al deshabilitar timer: %s", esp_err_to_name(err));
    }

    // Eliminar timer
    err = gptimer_del_timer(s_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al eliminar timer: %s", esp_err_to_name(err));
        return err;
    }

    s_pulse_timer = NULL;

    // Apagar LED
    if (s_gpio_num >= 0) {
        gpio_set_level(s_gpio_num, 0);
        s_gpio_num = -1;
    }

    ESP_LOGI(TAG, "LED manager desinicializado");
    return ESP_OK;
}

esp_err_t led_manager_set_level(bool level)
{
    if (s_gpio_num < 0) {
        ESP_LOGE(TAG, "LED manager no inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    gpio_set_level(s_gpio_num, level ? 1 : 0);
    return ESP_OK;
}

esp_err_t led_manager_pulse(uint32_t duration_ms)
{
    if (s_gpio_num < 0 || s_pulse_timer == NULL) {
        ESP_LOGE(TAG, "LED manager no inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    if (duration_ms == 0) {
        ESP_LOGE(TAG, "La duración debe ser mayor que 0");
        return ESP_ERR_INVALID_ARG;
    }

    // Detener timer si está corriendo
    gptimer_stop(s_pulse_timer);
    
    // CRÍTICO: Resetear el contador del timer a 0
    esp_err_t err = gptimer_set_raw_count(s_pulse_timer, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al resetear contador: %s", esp_err_to_name(err));
        return err;
    }

    // Configurar alarma one-shot
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = duration_ms * 1000, // convertir ms a microsegundos
        .flags.auto_reload_on_alarm = false, // one-shot
    };
    
    err = gptimer_set_alarm_action(s_pulse_timer, &alarm_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al configurar alarma: %s", esp_err_to_name(err));
        gpio_set_level(s_gpio_num, 0);
        return err;
    }

    // Encender LED
    gpio_set_level(s_gpio_num, 1);

    // Iniciar timer
    err = gptimer_start(s_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar timer: %s", esp_err_to_name(err));
        gpio_set_level(s_gpio_num, 0);
        return err;
    }

    return ESP_OK;
}
