#pragma once

#include <stdbool.h>
#include <stdint.h>

#define APP_MATRIX_WIDTH 8
#define APP_MATRIX_HEIGHT 8
#define APP_MAX_SSID_LEN 32
#define APP_MAX_PASSWORD_LEN 64
#define APP_MAX_PROVIDER_URL_LEN 128
#define APP_MAX_PROVIDER_KEY_LEN 64

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_pixel_t;

typedef enum {
    APP_TILT_FLAT = 0,
    APP_TILT_LEFT,
    APP_TILT_RIGHT,
    APP_TILT_UP,
    APP_TILT_DOWN
} app_tilt_direction_t;

typedef enum {
    APP_WIFI_STATE_IDLE = 0,
    APP_WIFI_STATE_CONNECTING,
    APP_WIFI_STATE_CONNECTED,
    APP_WIFI_STATE_AP_PORTAL
} app_wifi_state_t;

typedef struct {
    char ssid[APP_MAX_SSID_LEN + 1];
    char password[APP_MAX_PASSWORD_LEN + 1];
    char ap_ssid[APP_MAX_SSID_LEN + 1];
    char ap_password[APP_MAX_PASSWORD_LEN + 1];
    uint8_t brightness_limit;
    bool matrix_serpentine;
    uint8_t matrix_rotation;
    uint8_t startup_effect_id;
    float tilt_threshold_g;
    float shake_threshold_delta_g;
    float accel_offset_x;
    float accel_offset_y;
    float accel_offset_z;
    bool provider_enabled;
    char provider_endpoint[APP_MAX_PROVIDER_URL_LEN + 1];
    char provider_api_key[APP_MAX_PROVIDER_KEY_LEN + 1];
} app_config_t;

typedef struct {
    int condition_code;
    uint32_t last_update_ms;
    char status_text[32];
    bool healthy;
} provider_snapshot_t;
