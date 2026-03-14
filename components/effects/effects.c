#include "effects.h"

#include <math.h>
#include <string.h>

static rgb_pixel_t s_framebuffer[APP_MATRIX_WIDTH * APP_MATRIX_HEIGHT];
static uint16_t s_phase;
static uint8_t s_sparkle_seed;
static float s_ball_x;
static float s_ball_y;
static float s_ball_vx;
static float s_ball_vy;
static uint32_t s_cycle_time_ms;

#define SCENE_SWIRL_DURATION_MS 10000U
#define SCENE_RAINBOW_DURATION_MS 10000U
#define SCENE_WINDOWS_DURATION_MS 10000U
#define LETTER_WIDTH 3U
#define LETTER_HEIGHT 5U
#define LETTER_SPACING 1U
#define LETTER_SCROLL_STEP_MS 80U
#define SCROLL_CHAR_COUNT 36U

static const char s_scroll_chars[SCROLL_CHAR_COUNT + 1U] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static const uint8_t s_glyphs[SCROLL_CHAR_COUNT][LETTER_WIDTH] = {
    {0x1e, 0x05, 0x1e}, {0x1f, 0x15, 0x0a}, {0x0e, 0x11, 0x11}, {0x1f, 0x11, 0x0e},
    {0x1f, 0x15, 0x11}, {0x1f, 0x05, 0x01}, {0x0e, 0x11, 0x1d}, {0x1f, 0x04, 0x1f},
    {0x11, 0x1f, 0x11}, {0x08, 0x10, 0x0f}, {0x1f, 0x04, 0x1b}, {0x1f, 0x10, 0x10},
    {0x1f, 0x06, 0x1f}, {0x1f, 0x0e, 0x1f}, {0x0e, 0x11, 0x0e}, {0x1f, 0x05, 0x02},
    {0x0e, 0x19, 0x1e}, {0x1f, 0x0d, 0x16}, {0x12, 0x15, 0x09}, {0x01, 0x1f, 0x01},
    {0x0f, 0x10, 0x0f}, {0x07, 0x18, 0x07}, {0x1f, 0x0c, 0x1f}, {0x1b, 0x04, 0x1b},
    {0x03, 0x1c, 0x03}, {0x19, 0x15, 0x13}, {0x0e, 0x11, 0x0e}, {0x12, 0x1f, 0x10},
    {0x19, 0x15, 0x12}, {0x11, 0x15, 0x0a}, {0x07, 0x04, 0x1f}, {0x17, 0x15, 0x09},
    {0x0e, 0x15, 0x08}, {0x01, 0x1d, 0x03}, {0x0a, 0x15, 0x0a}, {0x02, 0x15, 0x0e},
};

static rgb_pixel_t hsv_to_rgb(uint16_t hue, uint8_t sat, uint8_t val)
{
    uint8_t region = (uint8_t)(hue / 43U);
    uint8_t remainder = (uint8_t)((hue - (region * 43U)) * 6U);
    uint8_t p = (uint8_t)((val * (255U - sat)) >> 8U);
    uint8_t q = (uint8_t)((val * (255U - ((sat * remainder) >> 8U))) >> 8U);
    uint8_t t = (uint8_t)((val * (255U - ((sat * (255U - remainder)) >> 8U))) >> 8U);

    switch (region) {
    default:
    case 0:
        return (rgb_pixel_t){val, t, p};
    case 1:
        return (rgb_pixel_t){q, val, p};
    case 2:
        return (rgb_pixel_t){p, val, t};
    case 3:
        return (rgb_pixel_t){p, q, val};
    case 4:
        return (rgb_pixel_t){t, p, val};
    case 5:
        return (rgb_pixel_t){val, p, q};
    }
}

static void clear_framebuffer(void)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
}

static void fade_framebuffer(uint8_t keep)
{
    for (size_t i = 0; i < APP_MATRIX_WIDTH * APP_MATRIX_HEIGHT; ++i) {
        s_framebuffer[i].r = (uint8_t)(((uint16_t)s_framebuffer[i].r * keep) / 255U);
        s_framebuffer[i].g = (uint8_t)(((uint16_t)s_framebuffer[i].g * keep) / 255U);
        s_framebuffer[i].b = (uint8_t)(((uint16_t)s_framebuffer[i].b * keep) / 255U);
    }
}

static uint8_t clamp_u8(int value)
{
    if (value < 0) {
        return 0U;
    }
    if (value > 255) {
        return 255U;
    }
    return (uint8_t)value;
}

static void blend_pixel(rgb_pixel_t *pixel, rgb_pixel_t color)
{
    pixel->r = clamp_u8((int)pixel->r + color.r);
    pixel->g = clamp_u8((int)pixel->g + color.g);
    pixel->b = clamp_u8((int)pixel->b + color.b);
}

static rgb_pixel_t scale_rgb(rgb_pixel_t color, uint8_t scale)
{
    return (rgb_pixel_t){
        .r = (uint8_t)(((uint16_t)color.r * scale) / 255U),
        .g = (uint8_t)(((uint16_t)color.g * scale) / 255U),
        .b = (uint8_t)(((uint16_t)color.b * scale) / 255U),
    };
}

static void set_pixel(int x, int y, rgb_pixel_t color)
{
    if (x < 0 || x >= APP_MATRIX_WIDTH || y < 0 || y >= APP_MATRIX_HEIGHT) {
        return;
    }
    s_framebuffer[(y * APP_MATRIX_WIDTH) + x] = color;
}

static void plot_swirl_point(int x, int y, rgb_pixel_t color)
{
    if (x < 0 || x >= APP_MATRIX_WIDTH || y < 0 || y >= APP_MATRIX_HEIGHT) {
        return;
    }
    blend_pixel(&s_framebuffer[(y * APP_MATRIX_WIDTH) + x], color);
}

static void plot_swirl_point_soft(float x, float y, rgb_pixel_t color)
{
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    float fx = x - (float)x0;
    float fy = y - (float)y0;

    uint8_t w00 = (uint8_t)lroundf((1.0f - fx) * (1.0f - fy) * 255.0f);
    uint8_t w10 = (uint8_t)lroundf(fx * (1.0f - fy) * 255.0f);
    uint8_t w01 = (uint8_t)lroundf((1.0f - fx) * fy * 255.0f);
    uint8_t w11 = (uint8_t)lroundf(fx * fy * 255.0f);

    plot_swirl_point(x0, y0, scale_rgb(color, w00));
    plot_swirl_point(x0 + 1, y0, scale_rgb(color, w10));
    plot_swirl_point(x0, y0 + 1, scale_rgb(color, w01));
    plot_swirl_point(x0 + 1, y0 + 1, scale_rgb(color, w11));
}

static void render_swirl_scene(uint32_t scene_ms)
{
    const float tau = 6.2831853f;
    float t = (float)scene_ms * 0.00135f;
    float radius_x = ((float)APP_MATRIX_WIDTH - 1.0f) * 0.48f;
    float radius_y = ((float)APP_MATRIX_HEIGHT - 1.0f) * 0.48f;
    float center_x = ((float)APP_MATRIX_WIDTH - 1.0f) * 0.5f;
    float center_y = ((float)APP_MATRIX_HEIGHT - 1.0f) * 0.5f;
    uint16_t base_hue = (uint16_t)((scene_ms * 3U) / 48U);

    fade_framebuffer(232U);

    for (uint8_t i = 0; i < 6U; ++i) {
        float offset = ((float)i * (tau / 6.0f));
        float angle = t + offset;
        float x = center_x + (sinf(angle) * radius_x);
        float y = center_y + (cosf((t * 1.1f) - offset) * radius_y);
        uint16_t hue = (uint16_t)((base_hue + (i * 18U)) % 256U);
        rgb_pixel_t lead_color = hsv_to_rgb(hue, 180U, 150U);
        rgb_pixel_t mirror_x_color = hsv_to_rgb((uint16_t)((hue + 12U) % 256U), 168U, 132U);
        rgb_pixel_t mirror_y_color = hsv_to_rgb((uint16_t)((hue + 24U) % 256U), 156U, 120U);

        plot_swirl_point_soft(x, y, lead_color);
        plot_swirl_point_soft(((float)APP_MATRIX_WIDTH - 1.0f) - x, y, mirror_x_color);
        plot_swirl_point_soft(x, ((float)APP_MATRIX_HEIGHT - 1.0f) - y, mirror_y_color);
        plot_swirl_point_soft(x, y, scale_rgb(lead_color, 96U));
        plot_swirl_point_soft(((float)APP_MATRIX_WIDTH - 1.0f) - x, y, scale_rgb(mirror_x_color, 72U));
        plot_swirl_point_soft(x, ((float)APP_MATRIX_HEIGHT - 1.0f) - y, scale_rgb(mirror_y_color, 72U));
    }
}

static void render_rainbow_diagonal_scene(uint32_t scene_ms)
{
    clear_framebuffer();

    uint16_t sweep = (uint16_t)((scene_ms * 3U) / 20U);
    for (uint8_t y = 0; y < APP_MATRIX_HEIGHT; ++y) {
        for (uint8_t x = 0; x < APP_MATRIX_WIDTH; ++x) {
            uint16_t hue = (uint16_t)((sweep + ((x + y) * 18U)) % 256U);
            uint8_t value = (uint8_t)(96U + (((x + y) % 4U) * 16U));
            s_framebuffer[(y * APP_MATRIX_WIDTH) + x] = hsv_to_rgb(hue, 230U, value);
        }
    }
}

static void render_image_bitmap(const char *bitmap[APP_MATRIX_HEIGHT], int x_offset)
{
    rgb_pixel_t black = {0, 0, 0};
    rgb_pixel_t lime = {170, 255, 0};
    rgb_pixel_t white = {235, 235, 235};
    rgb_pixel_t gray = {175, 175, 175};
    rgb_pixel_t dark = {78, 78, 78};
    rgb_pixel_t red = {210, 40, 30};
    rgb_pixel_t ember = {255, 64, 48};
    rgb_pixel_t orange = {255, 145, 48};
    rgb_pixel_t yellow = {255, 220, 88};
    rgb_pixel_t warm_white = {255, 244, 214};

    for (int y = 0; y < APP_MATRIX_HEIGHT; ++y) {
        for (int x = 0; x < APP_MATRIX_WIDTH; ++x) {
            rgb_pixel_t color = black;
            switch (bitmap[y][x]) {
            case 'G':
                color = lime;
                break;
            case 'W':
                color = white;
                break;
            case 'M':
                color = gray;
                break;
            case 'D':
                color = dark;
                break;
            case 'K':
                color = black;
                break;
            case 'R':
                color = red;
                break;
            case 'E':
                color = ember;
                break;
            case 'O':
                color = orange;
                break;
            case 'Y':
                color = yellow;
                break;
            case 'H':
                color = warm_white;
                break;
            default:
                break;
            }
            set_pixel(x + x_offset, y, color);
        }
    }
}

static void render_windows_icon_scene(uint32_t scene_ms)
{
    clear_framebuffer();

    static const char *image_a[APP_MATRIX_HEIGHT] = {
        "GGGGGGGG",
        "GGWWWWGG",
        "GGWBWBWG",
        "GGWWWWWG",
        "GGMMMMMG",
        "GGWMMMMG",
        "GGDGGDGG",
        "GGDGGDGG",
    };

    static const char *image_b[APP_MATRIX_HEIGHT] = {
        "KKKRKKKK",
        "KKKRRKKK",
        "KKRROKKK",
        "KRRRORKK",
        "KRRYYRKK",
        "KROYYORK",
        "KRRHYORK",
        "KKRHHROK",
    };

    if (scene_ms < 4000U) {
        render_image_bitmap(image_a, 0);
    } else if (scene_ms < 6000U) {
        int shift = (int)(((scene_ms - 4000U) * APP_MATRIX_WIDTH) / 2000U);
        render_image_bitmap(image_a, -shift);
        render_image_bitmap(image_b, (int)APP_MATRIX_WIDTH - shift);
    } else {
        render_image_bitmap(image_b, 0);
    }
}

static uint32_t alphabet_scene_duration_ms(void)
{
    uint32_t columns_per_letter = APP_MATRIX_WIDTH + LETTER_WIDTH + LETTER_SPACING;
    return SCROLL_CHAR_COUNT * columns_per_letter * LETTER_SCROLL_STEP_MS;
}

static const uint8_t *lookup_glyph(char glyph)
{
    for (size_t i = 0; i < SCROLL_CHAR_COUNT; ++i) {
        if (s_scroll_chars[i] == glyph) {
            return s_glyphs[i];
        }
    }
    return NULL;
}

static void render_glyph(char glyph, int x_offset, rgb_pixel_t color)
{
    const uint8_t *bitmap = lookup_glyph(glyph);
    if (bitmap == NULL) {
        return;
    }

    for (uint8_t column = 0; column < LETTER_WIDTH; ++column) {
        for (uint8_t row = 0; row < LETTER_HEIGHT; ++row) {
            if ((bitmap[column] >> row) & 0x01U) {
                set_pixel(x_offset + (int)column, (int)row + 1, color);
            }
        }
    }
}

static void render_alphabet_scene(uint32_t scene_ms)
{
    clear_framebuffer();

    uint32_t columns_per_letter = APP_MATRIX_WIDTH + LETTER_WIDTH + LETTER_SPACING;
    uint32_t step = scene_ms / LETTER_SCROLL_STEP_MS;
    uint32_t letter_index = step / columns_per_letter;
    uint32_t column_offset = step % columns_per_letter;

    if (letter_index >= SCROLL_CHAR_COUNT) {
        letter_index = SCROLL_CHAR_COUNT - 1U;
        column_offset = columns_per_letter - 1U;
    }

    int x_offset = (int)APP_MATRIX_WIDTH - (int)column_offset;
    char letter = s_scroll_chars[letter_index];
    uint16_t hue = (uint16_t)((letter_index * 9U + step * 2U) % 256U);
    rgb_pixel_t color = hsv_to_rgb(hue, 160U, 180U);

    render_glyph(letter, x_offset, color);
}

static void effect_static_update(rgb_pixel_t *framebuffer, const app_state_t *state, uint32_t dt_ms)
{
    (void)state;
    clear_framebuffer();
    s_phase = (uint16_t)((s_phase + dt_ms) % 6400U);

    uint8_t step = (uint8_t)(s_phase / 100U);
    uint8_t scan_x = step % APP_MATRIX_WIDTH;
    uint8_t scan_y = (step / APP_MATRIX_WIDTH) % APP_MATRIX_HEIGHT;

    for (uint8_t y = 0; y < APP_MATRIX_HEIGHT; ++y) {
        for (uint8_t x = 0; x < APP_MATRIX_WIDTH; ++x) {
            rgb_pixel_t color = {0, 0, 0};

            if (x == 0U && y == 0U) {
                color = (rgb_pixel_t){80, 0, 0};
            } else if (x == (APP_MATRIX_WIDTH - 1U) && y == 0U) {
                color = (rgb_pixel_t){0, 80, 0};
            } else if (x == 0U && y == (APP_MATRIX_HEIGHT - 1U)) {
                color = (rgb_pixel_t){0, 0, 80};
            } else if (x == (APP_MATRIX_WIDTH - 1U) && y == (APP_MATRIX_HEIGHT - 1U)) {
                color = (rgb_pixel_t){80, 40, 0};
            } else if (y == scan_y && x == scan_x) {
                color = (rgb_pixel_t){120, 120, 120};
            } else if (y == scan_y) {
                color = (rgb_pixel_t){48, 12, 12};
            } else if (x == scan_x) {
                color = (rgb_pixel_t){12, 12, 48};
            }
            framebuffer[(y * APP_MATRIX_WIDTH) + x] = color;
        }
    }
}

static void effect_swirl_update(rgb_pixel_t *framebuffer, const app_state_t *state, uint32_t dt_ms)
{
    (void)framebuffer;
    (void)state;
    uint32_t cycle_duration = SCENE_SWIRL_DURATION_MS +
                              SCENE_RAINBOW_DURATION_MS +
                              SCENE_WINDOWS_DURATION_MS +
                              alphabet_scene_duration_ms();

    s_phase = (uint16_t)((s_phase + dt_ms) % 65535U);
    s_cycle_time_ms = (s_cycle_time_ms + dt_ms) % cycle_duration;

    if (s_cycle_time_ms < SCENE_SWIRL_DURATION_MS) {
        render_swirl_scene(s_cycle_time_ms);
    } else if (s_cycle_time_ms < (SCENE_SWIRL_DURATION_MS + SCENE_RAINBOW_DURATION_MS)) {
        render_rainbow_diagonal_scene(s_cycle_time_ms - SCENE_SWIRL_DURATION_MS);
    } else if (s_cycle_time_ms < (SCENE_SWIRL_DURATION_MS + SCENE_RAINBOW_DURATION_MS + SCENE_WINDOWS_DURATION_MS)) {
        render_windows_icon_scene(s_cycle_time_ms - SCENE_SWIRL_DURATION_MS - SCENE_RAINBOW_DURATION_MS);
    } else {
        render_alphabet_scene(s_cycle_time_ms - SCENE_SWIRL_DURATION_MS - SCENE_RAINBOW_DURATION_MS - SCENE_WINDOWS_DURATION_MS);
    }
}

static void effect_tilt_update(rgb_pixel_t *framebuffer, const app_state_t *state, uint32_t dt_ms)
{
    clear_framebuffer();
    const float dt = (float)dt_ms / 1000.0f;
    const float accel_scale = 10.0f;
    const float damping = 0.90f;
    const float max_x = (float)(APP_MATRIX_WIDTH - 1U);
    const float max_y = (float)(APP_MATRIX_HEIGHT - 1U);

    s_ball_vx += state->accel_x_g * accel_scale * dt;
    s_ball_vy += state->accel_y_g * accel_scale * dt;
    s_ball_vx *= damping;
    s_ball_vy *= damping;

    s_ball_x += s_ball_vx;
    s_ball_y -= s_ball_vy;

    if (s_ball_x < 0.0f) {
        s_ball_x = 0.0f;
        s_ball_vx *= -0.55f;
    } else if (s_ball_x > max_x) {
        s_ball_x = max_x;
        s_ball_vx *= -0.55f;
    }

    if (s_ball_y < 0.0f) {
        s_ball_y = 0.0f;
        s_ball_vy *= -0.55f;
    } else if (s_ball_y > max_y) {
        s_ball_y = max_y;
        s_ball_vy *= -0.55f;
    }

    for (uint8_t y = 0; y < APP_MATRIX_HEIGHT; ++y) {
        for (uint8_t x = 0; x < APP_MATRIX_WIDTH; ++x) {
            float dx = s_ball_x - (float)x;
            float dy = s_ball_y - (float)y;
            float distance = sqrtf((dx * dx) + (dy * dy));
            if (distance <= 0.75f) {
                framebuffer[(y * APP_MATRIX_WIDTH) + x] = (rgb_pixel_t){120, 40, 0};
            } else if (distance <= 1.5f) {
                framebuffer[(y * APP_MATRIX_WIDTH) + x] = (rgb_pixel_t){20, 4, 0};
            } else {
                framebuffer[(y * APP_MATRIX_WIDTH) + x] = (rgb_pixel_t){0, 0, 1};
            }
        }
    }
}

static void effect_sparkle_update(rgb_pixel_t *framebuffer, const app_state_t *state, uint32_t dt_ms)
{
    (void)dt_ms;
    for (size_t i = 0; i < APP_MATRIX_WIDTH * APP_MATRIX_HEIGHT; ++i) {
        framebuffer[i].r = (uint8_t)(framebuffer[i].r * 7U / 8U);
        framebuffer[i].g = (uint8_t)(framebuffer[i].g * 7U / 8U);
        framebuffer[i].b = (uint8_t)(framebuffer[i].b * 7U / 8U);
    }

    if (state->shake_detected) {
        s_sparkle_seed += 17U;
    }
    size_t index = (size_t)((s_sparkle_seed + s_phase) % (APP_MATRIX_WIDTH * APP_MATRIX_HEIGHT));
    framebuffer[index] = (rgb_pixel_t){90, 90, 90};
    s_sparkle_seed += 11U;
}

static void effect_provider_update(rgb_pixel_t *framebuffer, const app_state_t *state, uint32_t dt_ms)
{
    (void)dt_ms;
    clear_framebuffer();
    uint8_t tone = state->provider_ready ? (uint8_t)(32U + (state->provider_snapshot.condition_code % 128)) : 10U;
    for (size_t i = 0; i < APP_MATRIX_WIDTH * APP_MATRIX_HEIGHT; ++i) {
        framebuffer[i] = (rgb_pixel_t){tone, (uint8_t)(tone / 2U), (uint8_t)(120U - (tone / 2U))};
    }
}

static void default_init(const app_config_t *config, const app_state_t *initial_state)
{
    (void)config;
    (void)initial_state;
    clear_framebuffer();
    s_phase = 0;
    s_sparkle_seed = 1;
    s_ball_x = (float)(APP_MATRIX_WIDTH - 1U) / 2.0f;
    s_ball_y = (float)(APP_MATRIX_HEIGHT - 1U) / 2.0f;
    s_ball_vx = 0.0f;
    s_ball_vy = 0.0f;
    s_cycle_time_ms = 0U;
}

static void default_on_event(const app_event_t *event, const app_state_t *state)
{
    (void)state;
    if (event->type == APP_EVENT_SHAKE_DETECTED) {
        s_sparkle_seed += 37U;
    }
}

static const app_effect_t s_effects[] = {
    {.id = 0, .name = "static", .init = default_init, .update = effect_static_update, .on_event = default_on_event},
    {.id = 1, .name = "swirl", .init = default_init, .update = effect_swirl_update, .on_event = default_on_event},
    {.id = 2, .name = "tilt", .init = default_init, .update = effect_tilt_update, .on_event = default_on_event},
    {.id = 3, .name = "sparkle", .init = default_init, .update = effect_sparkle_update, .on_event = default_on_event},
    {.id = 4, .name = "provider", .init = default_init, .update = effect_provider_update, .on_event = default_on_event},
};

void effect_registry_init(const app_config_t *config, const app_state_t *initial_state)
{
    for (size_t i = 0; i < sizeof(s_effects) / sizeof(s_effects[0]); ++i) {
        if (s_effects[i].init != NULL) {
            s_effects[i].init(config, initial_state);
        }
    }
}

void effect_registry_render_active(const app_state_t *state, uint32_t dt_ms)
{
    size_t index = state->active_effect_id % effect_registry_count();
    s_effects[index].update(s_framebuffer, state, dt_ms);
}

void effect_registry_handle_event(const app_state_t *state, const app_event_t *event)
{
    size_t index = state->active_effect_id % effect_registry_count();
    if (s_effects[index].on_event != NULL) {
        s_effects[index].on_event(event, state);
    }
}

size_t effect_registry_count(void)
{
    return sizeof(s_effects) / sizeof(s_effects[0]);
}

const rgb_pixel_t *effect_registry_get_framebuffer(void)
{
    return s_framebuffer;
}
