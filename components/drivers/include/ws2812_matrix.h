#pragma once

#include <stddef.h>
#include <stdint.h>

#include "app_types.h"
#include "esp_err.h"

esp_err_t ws2812_matrix_init(const app_config_t *config);
esp_err_t ws2812_matrix_show(const rgb_pixel_t *pixels, size_t count);
size_t ws2812_matrix_map_xy(uint8_t x, uint8_t y, const app_config_t *config);
