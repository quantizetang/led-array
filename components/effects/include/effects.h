#pragma once

#include <stddef.h>
#include <stdint.h>

#include "app_events.h"
#include "app_state.h"
#include "app_types.h"

typedef struct {
    uint8_t id;
    const char *name;
    void (*init)(const app_config_t *config, const app_state_t *initial_state);
    void (*update)(rgb_pixel_t *framebuffer, const app_state_t *state, uint32_t dt_ms);
    void (*on_event)(const app_event_t *event, const app_state_t *state);
} app_effect_t;

void effect_registry_init(const app_config_t *config, const app_state_t *initial_state);
void effect_registry_render_active(const app_state_t *state, uint32_t dt_ms);
void effect_registry_handle_event(const app_state_t *state, const app_event_t *event);
size_t effect_registry_count(void);
const rgb_pixel_t *effect_registry_get_framebuffer(void);
