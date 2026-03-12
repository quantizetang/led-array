#include "providers.h"

#include <stdio.h>
#include <string.h>

#include "app_events.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    QueueHandle_t queue;
    app_config_t config;
} provider_context_t;

static esp_err_t local_provider_init(const app_config_t *config)
{
    (void)config;
    return ESP_OK;
}

static esp_err_t local_provider_refresh(provider_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t tick_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    snapshot->condition_code = (int)(tick_ms / 5000U) % 5;
    snapshot->last_update_ms = tick_ms;
    snapshot->healthy = true;
    snprintf(snapshot->status_text, sizeof(snapshot->status_text), "demo-%d", snapshot->condition_code);
    return ESP_OK;
}

static const app_provider_t s_provider = {
    .name = "local_demo",
    .init = local_provider_init,
    .refresh = local_provider_refresh,
};

static void provider_task(void *arg)
{
    provider_context_t *ctx = (provider_context_t *)arg;
    provider_snapshot_t snapshot = {0};

    while (true) {
        if (ctx->config.provider_enabled) {
            app_event_t event = {0};
            if (s_provider.refresh(&snapshot) == ESP_OK) {
                event.type = APP_EVENT_PROVIDER_UPDATED;
                event.data.provider.snapshot = snapshot;
            } else {
                event.type = APP_EVENT_PROVIDER_ERROR;
            }
            xQueueSend(ctx->queue, &event, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

esp_err_t provider_manager_init(QueueHandle_t event_queue, const app_config_t *config)
{
    if ((event_queue == NULL) || (config == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(s_provider.init(config), "providers", "provider init failed");

    static provider_context_t context;
    memset(&context, 0, sizeof(context));
    context.queue = event_queue;
    context.config = *config;

    BaseType_t ok = xTaskCreate(provider_task, "provider_task", 3072, &context, 4, NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}
