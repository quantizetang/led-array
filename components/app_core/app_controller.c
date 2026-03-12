#include "app_controller.h"

#include <string.h>

#include "effects.h"

static void app_controller_apply_event_locked(app_controller_t *controller, const app_event_t *event)
{
    app_state_t *state = &controller->state;

    switch (event->type) {
    case APP_EVENT_TILT_CHANGED:
        state->tilt_direction = event->data.tilt.direction;
        state->accel_x_g = event->data.tilt.ax;
        state->accel_y_g = event->data.tilt.ay;
        state->accel_z_g = event->data.tilt.az;
        state->shake_detected = false;
        break;
    case APP_EVENT_SHAKE_DETECTED:
        state->shake_detected = true;
        break;
    case APP_EVENT_WIFI_CONNECTED:
        state->wifi_state = APP_WIFI_STATE_CONNECTED;
        break;
    case APP_EVENT_WIFI_DISCONNECTED:
        state->wifi_state = event->data.wifi.reconnecting ? APP_WIFI_STATE_CONNECTING : APP_WIFI_STATE_IDLE;
        break;
    case APP_EVENT_WIFI_PORTAL_STARTED:
        state->wifi_state = APP_WIFI_STATE_AP_PORTAL;
        break;
    case APP_EVENT_PROVIDER_UPDATED:
        state->provider_snapshot = event->data.provider.snapshot;
        state->provider_ready = true;
        break;
    case APP_EVENT_PROVIDER_ERROR:
        state->provider_ready = false;
        break;
    default:
        break;
    }

    state->event_counter++;
}

esp_err_t app_controller_init(app_controller_t *controller, const app_config_t *config)
{
    if ((controller == NULL) || (config == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(controller, 0, sizeof(*controller));
    controller->queue = xQueueCreate(16, sizeof(app_event_t));
    controller->lock = xSemaphoreCreateMutex();
    if ((controller->queue == NULL) || (controller->lock == NULL)) {
        return ESP_ERR_NO_MEM;
    }

    controller->config = *config;
    controller->state.active_effect_id = config->startup_effect_id % effect_registry_count();
    controller->state.wifi_state = APP_WIFI_STATE_IDLE;
    controller->state.tilt_direction = APP_TILT_FLAT;

    effect_registry_init(config, &controller->state);
    return ESP_OK;
}

void app_controller_process_events(app_controller_t *controller, TickType_t wait_ticks)
{
    if (controller == NULL) {
        return;
    }

    app_event_t event = {0};
    if (xQueueReceive(controller->queue, &event, wait_ticks) == pdTRUE) {
        xSemaphoreTake(controller->lock, portMAX_DELAY);
        app_controller_apply_event_locked(controller, &event);
        effect_registry_handle_event(&controller->state, &event);
        xSemaphoreGive(controller->lock);

        while (xQueueReceive(controller->queue, &event, 0) == pdTRUE) {
            xSemaphoreTake(controller->lock, portMAX_DELAY);
            app_controller_apply_event_locked(controller, &event);
            effect_registry_handle_event(&controller->state, &event);
            xSemaphoreGive(controller->lock);
        }
    }
}

void app_controller_get_state(app_controller_t *controller, app_state_t *out_state)
{
    if ((controller == NULL) || (out_state == NULL)) {
        return;
    }

    xSemaphoreTake(controller->lock, portMAX_DELAY);
    *out_state = controller->state;
    xSemaphoreGive(controller->lock);
}

QueueHandle_t app_controller_get_queue(app_controller_t *controller)
{
    return (controller == NULL) ? NULL : controller->queue;
}
