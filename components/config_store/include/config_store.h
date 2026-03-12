#pragma once

#include "app_types.h"
#include "esp_err.h"

esp_err_t config_store_init(void);
esp_err_t config_store_load(app_config_t *config);
esp_err_t config_store_save(const app_config_t *config);
