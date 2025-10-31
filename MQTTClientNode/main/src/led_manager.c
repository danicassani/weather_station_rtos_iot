#include "led_manager.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_MANAGER";

static int s_gpio_num = -1;
static gptimer_handle_t s_pulse_timer = NULL;

// Timer callback to turn off LED after pulse
static bool IRAM_ATTR pulse_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    gpio_set_level(s_gpio_num, 0);  // Turn off LED
    gptimer_stop(timer);
    return false;
}

esp_err_t led_manager_init(int gpio_num)
{
    if (gpio_num < 0) {
        ESP_LOGE(TAG, "Invalid GPIO number: %d", gpio_num);
        return ESP_ERR_INVALID_ARG;
    }

    s_gpio_num = gpio_num;

    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio_num,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", gpio_num, esp_err_to_name(err));
        return err;
    }

    // Start in LOW
    gpio_set_level(gpio_num, 0);
    ESP_LOGI(TAG, "GPIO %d configured as output", gpio_num);

    // Configure one-shot timer for pulses
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1 MHz, tick = 1 µs
    };
    
    err = gptimer_new_timer(&timer_config, &s_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(err));
        return err;
    }

    // Configure callback
    gptimer_event_callbacks_t cbs = {
        .on_alarm = pulse_timer_callback,
    };
    err = gptimer_register_event_callbacks(s_pulse_timer, &cbs, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register callback: %s", esp_err_to_name(err));
        gptimer_del_timer(s_pulse_timer);
        return err;
    }

    err = gptimer_enable(s_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable timer: %s", esp_err_to_name(err));
        gptimer_del_timer(s_pulse_timer);
        return err;
    }

    ESP_LOGI(TAG, "LED manager started: GPIO=%d", gpio_num);
    return ESP_OK;
}

esp_err_t led_manager_deinit(void)
{
    if (s_pulse_timer == NULL) {
        ESP_LOGW(TAG, "LED manager was not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;

    // Stop timer if running
    gptimer_stop(s_pulse_timer);

    // Disable timer
    err = gptimer_disable(s_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable timer: %s", esp_err_to_name(err));
    }

    // Delete timer
    err = gptimer_del_timer(s_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete timer: %s", esp_err_to_name(err));
        return err;
    }

    s_pulse_timer = NULL;

    // Turn off LED
    if (s_gpio_num >= 0) {
        gpio_set_level(s_gpio_num, 0);
        s_gpio_num = -1;
    }

    ESP_LOGI(TAG, "LED manager deinitialized");
    return ESP_OK;
}

esp_err_t led_manager_set_level(bool level)
{
    if (s_gpio_num < 0) {
        ESP_LOGE(TAG, "LED manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    gpio_set_level(s_gpio_num, level ? 1 : 0);
    return ESP_OK;
}

esp_err_t led_manager_pulse(uint32_t duration_ms)
{
    if (s_gpio_num < 0 || s_pulse_timer == NULL) {
        ESP_LOGE(TAG, "LED manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (duration_ms == 0) {
        ESP_LOGE(TAG, "Duration must be greater than 0");
        return ESP_ERR_INVALID_ARG;
    }

    // Stop timer if running
    gptimer_stop(s_pulse_timer);
    
    // CRITICAL: Reset timer counter to 0
    esp_err_t err = gptimer_set_raw_count(s_pulse_timer, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset counter: %s", esp_err_to_name(err));
        return err;
    }

    // Configure one-shot alarm
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = duration_ms * 1000, // convert ms to microseconds
        .flags.auto_reload_on_alarm = false, // one-shot
    };
    
    err = gptimer_set_alarm_action(s_pulse_timer, &alarm_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure alarm: %s", esp_err_to_name(err));
        gpio_set_level(s_gpio_num, 0);
        return err;
    }

    // Turn on LED
    gpio_set_level(s_gpio_num, 1);

    // Start timer
    err = gptimer_start(s_pulse_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(err));
        gpio_set_level(s_gpio_num, 0);
        return err;
    }

    return ESP_OK;
}
