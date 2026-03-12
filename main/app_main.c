#include "app_controller.h"
#include "config_store.h"
#include "effects.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mpu6050_driver.h"
#include "providers.h"
#include "wifi_portal.h"
#include "ws2812_matrix.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "booting led_array_platform");

    esp_err_t err = config_store_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config_store_init failed: %s", esp_err_to_name(err));
        return;
    }

    app_config_t config = {0};
    err = config_store_load(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config_store_load failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG,
             "config loaded: startup_effect=%u brightness=%u rotation=%u serpentine=%s wifi_ssid_saved=%s ap_ssid=%s",
             config.startup_effect_id,
             config.brightness_limit,
             config.matrix_rotation,
             config.matrix_serpentine ? "true" : "false",
             strlen(config.ssid) > 0U ? "true" : "false",
             config.ap_ssid);

    app_controller_t controller = {0};
    err = app_controller_init(&controller, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "app_controller_init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "controller initialized");

    err = ws2812_matrix_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ws2812_matrix_init failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "ws2812 matrix initialized on GPIO7");
    }

    err = mpu6050_driver_start(app_controller_get_queue(&controller), &config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mpu6050_driver_start degraded: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "mpu6050 task started on I2C SDA=GPIO5 SCL=GPIO6");
    }

    err = provider_manager_init(app_controller_get_queue(&controller), &config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "provider_manager_init degraded: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "provider manager started");
    }

    err = wifi_portal_start(app_controller_get_queue(&controller), &config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "wifi_portal_start degraded: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "wifi manager started");
    }

    const TickType_t render_delay = pdMS_TO_TICKS(33);
    int64_t last_frame_us = esp_timer_get_time();
    app_wifi_state_t last_wifi_state = APP_WIFI_STATE_IDLE;

    while (true) {
        app_controller_process_events(&controller, 0);

        int64_t now_us = esp_timer_get_time();
        uint32_t dt_ms = (uint32_t)((now_us - last_frame_us) / 1000);
        if (dt_ms == 0U) {
            dt_ms = 1U;
        }
        last_frame_us = now_us;

        app_state_t state_snapshot = {0};
        app_controller_get_state(&controller, &state_snapshot);
        if (state_snapshot.wifi_state == APP_WIFI_STATE_AP_PORTAL && last_wifi_state != APP_WIFI_STATE_AP_PORTAL) {
            ESP_LOGI(TAG, "app state entered captive portal / hotspot mode");
            wifi_portal_force_config_mode();
        }
        last_wifi_state = state_snapshot.wifi_state;
        effect_registry_render_active(&state_snapshot, dt_ms);
        ws2812_matrix_show(effect_registry_get_framebuffer(), APP_MATRIX_WIDTH * APP_MATRIX_HEIGHT);

        vTaskDelay(render_delay);
    }
}
