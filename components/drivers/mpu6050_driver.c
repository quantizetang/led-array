#include "mpu6050_driver.h"

#include <math.h>
#include <string.h>

#include "app_events.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MPU6050_I2C_PORT 0
#define MPU6050_ADDR 0x68
#define MPU6050_GPIO_SDA 5
#define MPU6050_GPIO_SCL 6
#define MPU6050_WHO_AM_I_REG 0x75
#define MPU6050_PWR_MGMT_1_REG 0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B

typedef struct {
    QueueHandle_t queue;
    app_config_t config;
    i2c_master_dev_handle_t dev;
    int64_t last_log_us;
} mpu_context_t;

static const char *TAG = "mpu6050";

static esp_err_t mpu_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value)
{
    uint8_t payload[2] = {reg, value};
    return i2c_master_transmit(dev, payload, sizeof(payload), -1);
}

static esp_err_t mpu_read_regs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, -1);
}

static app_tilt_direction_t classify_tilt(float ax, float ay, float threshold)
{
    if (ax > threshold) {
        return APP_TILT_RIGHT;
    }
    if (ax < -threshold) {
        return APP_TILT_LEFT;
    }
    if (ay > threshold) {
        return APP_TILT_UP;
    }
    if (ay < -threshold) {
        return APP_TILT_DOWN;
    }
    return APP_TILT_FLAT;
}

static void mpu_task(void *arg)
{
    mpu_context_t *ctx = (mpu_context_t *)arg;
    app_tilt_direction_t last_direction = APP_TILT_FLAT;
    float last_magnitude = 1.0f;

    while (true) {
        uint8_t raw[6] = {0};
        esp_err_t err = mpu_read_regs(ctx->dev, MPU6050_ACCEL_XOUT_H, raw, sizeof(raw));
        if (err == ESP_OK) {
            int16_t ax_raw = (int16_t)((raw[0] << 8) | raw[1]);
            int16_t ay_raw = (int16_t)((raw[2] << 8) | raw[3]);
            int16_t az_raw = (int16_t)((raw[4] << 8) | raw[5]);

            float ax = ((float)ax_raw / 16384.0f) + ctx->config.accel_offset_x;
            float ay = ((float)ay_raw / 16384.0f) + ctx->config.accel_offset_y;
            float az = ((float)az_raw / 16384.0f) + ctx->config.accel_offset_z;
            float magnitude = sqrtf((ax * ax) + (ay * ay) + (az * az));
            int64_t now_us = esp_timer_get_time();

            if ((now_us - ctx->last_log_us) >= 1000000LL) {
                ESP_LOGI(TAG, "reading ax=%.3fg ay=%.3fg az=%.3fg magnitude=%.3fg", ax, ay, az, magnitude);
                ctx->last_log_us = now_us;
            }

            app_tilt_direction_t direction = classify_tilt(ax, ay, ctx->config.tilt_threshold_g);
            if (direction != last_direction) {
                ESP_LOGI(TAG, "tilt changed: dir=%d ax=%.3fg ay=%.3fg az=%.3fg", direction, ax, ay, az);
                app_event_t event = {
                    .type = APP_EVENT_TILT_CHANGED,
                    .data.tilt = {
                        .direction = direction,
                        .ax = ax,
                        .ay = ay,
                        .az = az,
                    },
                };
                xQueueSend(ctx->queue, &event, 0);
                last_direction = direction;
            }

            if (fabsf(magnitude - last_magnitude) >= ctx->config.shake_threshold_delta_g) {
                ESP_LOGI(TAG, "shake detected: delta=%.3fg threshold=%.3fg", fabsf(magnitude - last_magnitude), ctx->config.shake_threshold_delta_g);
                app_event_t event = {.type = APP_EVENT_SHAKE_DETECTED};
                xQueueSend(ctx->queue, &event, 0);
            }
            last_magnitude = magnitude;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t mpu6050_driver_start(QueueHandle_t event_queue, const app_config_t *config)
{
    if ((event_queue == NULL) || (config == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = MPU6050_I2C_PORT,
        .scl_io_num = MPU6050_GPIO_SCL,
        .sda_io_num = MPU6050_GPIO_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle = NULL;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus_handle), TAG, "i2c bus create failed");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_ADDR,
        .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t dev_handle = NULL;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle), TAG, "mpu add device failed");

    uint8_t who_am_i = 0;
    ESP_RETURN_ON_ERROR(mpu_read_regs(dev_handle, MPU6050_WHO_AM_I_REG, &who_am_i, 1), TAG, "mpu probe failed");
    ESP_LOGI(TAG, "mpu6050 probe ok, WHO_AM_I=0x%02x", who_am_i);
    if (who_am_i != MPU6050_ADDR) {
        ESP_LOGW(TAG, "unexpected WHO_AM_I=0x%02x", who_am_i);
    }

    ESP_RETURN_ON_ERROR(mpu_write_reg(dev_handle, MPU6050_PWR_MGMT_1_REG, 0x00), TAG, "mpu wake failed");
    ESP_LOGI(TAG, "mpu6050 wake complete");

    static mpu_context_t context;
    memset(&context, 0, sizeof(context));
    context.queue = event_queue;
    context.config = *config;
    context.dev = dev_handle;
    context.last_log_us = 0;

    BaseType_t ok = xTaskCreate(mpu_task, "mpu_task", 4096, &context, 5, NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}
