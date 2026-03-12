#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"

typedef struct {
    uint8_t active_effect_id;
    app_tilt_direction_t tilt_direction;
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    bool shake_detected;
    app_wifi_state_t wifi_state;
    bool provider_ready;
    provider_snapshot_t provider_snapshot;
    uint32_t event_counter;
} app_state_t;
