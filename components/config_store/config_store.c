#include "config_store.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "config_store";
static const char *NAMESPACE = "appcfg";
static const char *KEY_BLOB = "settings";

static void config_set_defaults(app_config_t *config)
{
    memset(config, 0, sizeof(*config));
    strlcpy(config->ap_ssid, "LED-Array-Setup", sizeof(config->ap_ssid));
    strlcpy(config->ap_password, "ledarray123", sizeof(config->ap_password));
    config->brightness_limit = 5;
    config->matrix_serpentine = false;
    config->matrix_rotation = 0;
    config->startup_effect_id = 1;
    config->tilt_threshold_g = 0.30f;
    config->shake_threshold_delta_g = 0.90f;
    config->provider_enabled = false;
    strlcpy(config->provider_endpoint, "local://demo", sizeof(config->provider_endpoint));
}

esp_err_t config_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs init failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t config_store_load(app_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    config_set_defaults(config);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    size_t size = sizeof(*config);
    err = nvs_get_blob(handle, KEY_BLOB, config, &size);
    nvs_close(handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        config_set_defaults(config);
        return ESP_OK;
    }
    if (err != ESP_OK || size != sizeof(*config)) {
        ESP_LOGW(TAG, "invalid stored config, using defaults");
        config_set_defaults(config);
        return ESP_OK;
    }

    config->brightness_limit = 5;
    config->startup_effect_id = 1;

    return ESP_OK;
}

esp_err_t config_store_save(const app_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, KEY_BLOB, config, sizeof(*config));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}
