#include "ws2812_matrix.h"

#include "led_strip.h"

#define WS2812_GPIO 7
#define WS2812_HARD_BRIGHTNESS_CAP 16

static led_strip_handle_t s_led_strip;
static app_config_t s_config;

static uint8_t effective_brightness_limit(uint8_t configured_limit)
{
    return (configured_limit > WS2812_HARD_BRIGHTNESS_CAP) ? WS2812_HARD_BRIGHTNESS_CAP : configured_limit;
}

static uint8_t scale_channel(uint8_t value, uint8_t limit)
{
    return (uint8_t)(((uint16_t)value * limit) / 255U);
}

esp_err_t ws2812_matrix_init(const app_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_config = *config;

    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_GPIO,
        .max_leds = APP_MATRIX_WIDTH * APP_MATRIX_HEIGHT,
        .led_model = LED_MODEL_WS2812,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (err == ESP_OK) {
        led_strip_clear(s_led_strip);
    }
    return err;
}

size_t ws2812_matrix_map_xy(uint8_t x, uint8_t y, const app_config_t *config)
{
    app_config_t effective = s_config;
    if (config != NULL) {
        effective = *config;
    }

    uint8_t rx = x;
    uint8_t ry = y;
    switch (effective.matrix_rotation % 4U) {
    case 1:
        rx = (APP_MATRIX_HEIGHT - 1U) - y;
        ry = x;
        break;
    case 2:
        rx = (APP_MATRIX_WIDTH - 1U) - x;
        ry = (APP_MATRIX_HEIGHT - 1U) - y;
        break;
    case 3:
        rx = y;
        ry = (APP_MATRIX_WIDTH - 1U) - x;
        break;
    default:
        break;
    }

    if (effective.matrix_serpentine && ((ry % 2U) == 1U)) {
        rx = (APP_MATRIX_WIDTH - 1U) - rx;
    }
    return ((size_t)ry * APP_MATRIX_WIDTH) + rx;
}

esp_err_t ws2812_matrix_show(const rgb_pixel_t *pixels, size_t count)
{
    if ((s_led_strip == NULL) || (pixels == NULL) || (count < (APP_MATRIX_WIDTH * APP_MATRIX_HEIGHT))) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t limit = effective_brightness_limit(s_config.brightness_limit);

    for (uint8_t y = 0; y < APP_MATRIX_HEIGHT; ++y) {
        for (uint8_t x = 0; x < APP_MATRIX_WIDTH; ++x) {
            size_t logical = ((size_t)y * APP_MATRIX_WIDTH) + x;
            size_t physical = ws2812_matrix_map_xy(x, y, &s_config);
            rgb_pixel_t pixel = pixels[logical];
            led_strip_set_pixel(s_led_strip,
                                physical,
                                scale_channel(pixel.r, limit),
                                scale_channel(pixel.g, limit),
                                scale_channel(pixel.b, limit));
        }
    }

    return led_strip_refresh(s_led_strip);
}
