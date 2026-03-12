#pragma once

#include "app_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t mpu6050_driver_start(QueueHandle_t event_queue, const app_config_t *config);
