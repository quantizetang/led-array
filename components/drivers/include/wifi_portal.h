#pragma once

#include "app_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t wifi_portal_start(QueueHandle_t event_queue, const app_config_t *config);
esp_err_t wifi_portal_force_config_mode(void);
