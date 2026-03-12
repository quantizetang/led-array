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

static void effect_static_update(rgb_pixel_t *framebuffer, const app_state_t *state, uint32_t dt_ms)
{
    (void)state;
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

static void effect_rainbow_update(rgb_pixel_t *framebuffer, const app_state_t *state, uint32_t dt_ms)
{
    (void)state;
    s_phase = (uint16_t)((s_phase + dt_ms) % 256U);
    for (uint8_t y = 0; y < APP_MATRIX_HEIGHT; ++y) {
        for (uint8_t x = 0; x < APP_MATRIX_WIDTH; ++x) {
            uint16_t hue = (uint16_t)((s_phase + (x * 16U) + (y * 8U)) % 256U);
            framebuffer[(y * APP_MATRIX_WIDTH) + x] = hsv_to_rgb(hue, 255, 80);
        }
    }
}

static void effect_tilt_update(rgb_pixel_t *framebuffer, const app_state_t *state, uint32_t dt_ms)
{
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
    {.id = 1, .name = "rainbow", .init = default_init, .update = effect_rainbow_update, .on_event = default_on_event},
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
    clear_framebuffer();
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
