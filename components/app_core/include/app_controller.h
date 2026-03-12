#pragma once

#include "app_events.h"
#include "app_state.h"
#include "app_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

typedef struct {
    QueueHandle_t queue;
    SemaphoreHandle_t lock;
    app_state_t state;
    app_config_t config;
} app_controller_t;

esp_err_t app_controller_init(app_controller_t *controller, const app_config_t *config);
void app_controller_process_events(app_controller_t *controller, TickType_t wait_ticks);
void app_controller_get_state(app_controller_t *controller, app_state_t *out_state);
QueueHandle_t app_controller_get_queue(app_controller_t *controller);
