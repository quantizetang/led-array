#pragma once

#include "app_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    const char *name;
    esp_err_t (*init)(const app_config_t *config);
    esp_err_t (*refresh)(provider_snapshot_t *snapshot);
} app_provider_t;

esp_err_t provider_manager_init(QueueHandle_t event_queue, const app_config_t *config);
