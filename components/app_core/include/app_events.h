#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app_types.h"

typedef enum {
    APP_EVENT_NONE = 0,
    APP_EVENT_TILT_CHANGED,
    APP_EVENT_SHAKE_DETECTED,
    APP_EVENT_WIFI_CONNECTED,
    APP_EVENT_WIFI_DISCONNECTED,
    APP_EVENT_WIFI_PORTAL_STARTED,
    APP_EVENT_PROVIDER_UPDATED,
    APP_EVENT_PROVIDER_ERROR
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    union {
        struct {
            app_tilt_direction_t direction;
            float ax;
            float ay;
            float az;
        } tilt;
        struct {
            provider_snapshot_t snapshot;
        } provider;
        struct {
            bool reconnecting;
        } wifi;
    } data;
} app_event_t;
