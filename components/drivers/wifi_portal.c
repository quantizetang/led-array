#include "wifi_portal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "app_events.h"
#include "config_store.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

static QueueHandle_t s_event_queue;
static app_config_t s_config;
static httpd_handle_t s_http_server;
static TaskHandle_t s_dns_task;
static bool s_portal_active;
static bool s_wifi_started;
static uint8_t s_sta_retry_count;
static const uint8_t s_sta_retry_limit = 5;
static const bool s_force_ap_only = true;
static const char *TAG = "wifi_portal";

static esp_err_t start_station(void);
static esp_err_t start_ap_portal(void);

static const char PORTAL_HTML[] =
    "<!doctype html><html><head><title>LED Array Setup</title>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>body{font-family:sans-serif;max-width:28rem;margin:2rem auto;padding:1rem;}"
    "input{width:100%;padding:0.7rem;margin:0.4rem 0;}button{padding:0.8rem 1rem;}</style>"
    "</head><body><h1>LED Array Setup</h1><form method='post' action='/save'>"
    "<label>Wi-Fi SSID</label><input name='ssid' maxlength='32'/>"
    "<label>Password</label><input type='password' name='password' maxlength='64'/>"
    "<button type='submit'>Save and Connect</button></form></body></html>";

static void url_decode(char *text)
{
    char *src = text;
    char *dst = text;
    while (*src != '\0') {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static bool form_get_value(char *body, const char *key, char *out, size_t out_len)
{
    char *cursor = body;
    size_t key_len = strlen(key);
    while (cursor != NULL && *cursor != '\0') {
        char *next = strchr(cursor, '&');
        if (next != NULL) {
            *next = '\0';
        }

        if (strncmp(cursor, key, key_len) == 0 && cursor[key_len] == '=') {
            strlcpy(out, cursor + key_len + 1, out_len);
            url_decode(out);
            if (next != NULL) {
                *next = '&';
            }
            return true;
        }

        if (next != NULL) {
            *next = '&';
            cursor = next + 1;
        } else {
            cursor = NULL;
        }
    }
    return false;
}

static esp_err_t portal_index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PORTAL_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t portal_save_handler(httpd_req_t *req)
{
    char body[256] = {0};
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        return ESP_FAIL;
    }
    body[received] = '\0';

    app_config_t updated = s_config;
    bool should_reconnect = false;
    form_get_value(body, "ssid", updated.ssid, sizeof(updated.ssid));
    form_get_value(body, "password", updated.password, sizeof(updated.password));

    if (strlen(updated.ssid) > 0U) {
        ESP_LOGI(TAG, "received Wi-Fi credentials from portal for SSID '%s'", updated.ssid);
        ESP_ERROR_CHECK(config_store_save(&updated));
        s_config = updated;
        should_reconnect = true;
    }

    httpd_resp_set_type(req, "text/html");
    esp_err_t err = httpd_resp_sendstr(req, "<html><body><h1>Saved</h1><p>Reconnecting with the new Wi-Fi settings.</p></body></html>");

    if (should_reconnect) {
        ESP_LOGI(TAG, "restarting Wi-Fi and reconnecting as station");
        esp_wifi_stop();
        s_wifi_started = false;
        s_portal_active = false;
        start_station();
    }

    return err;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "station started, attempting Wi-Fi connection");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_sta_retry_count++;
        if (s_sta_retry_count >= s_sta_retry_limit) {
            ESP_LOGW(TAG, "station retry limit reached (%u), enabling captive portal hotspot", s_sta_retry_limit);
            wifi_portal_force_config_mode();
        } else {
            ESP_LOGW(TAG, "station disconnected, retry %u/%u", s_sta_retry_count, s_sta_retry_limit);
            app_event_t event = {
                .type = APP_EVENT_WIFI_DISCONNECTED,
                .data.wifi = {.reconnecting = true},
            };
            xQueueSend(s_event_queue, &event, 0);
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_sta_retry_count = 0;
        ESP_LOGI(TAG, "station connected and received IP address");
        app_event_t event = {.type = APP_EVENT_WIFI_CONNECTED};
        xQueueSend(s_event_queue, &event, 0);
    }
}

static esp_err_t start_http_server(void)
{
    if (s_http_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    ESP_RETURN_ON_ERROR(httpd_start(&s_http_server, &config), TAG, "http start failed");

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = portal_index_handler,
    };
    httpd_uri_t save_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = portal_save_handler,
    };

    httpd_register_uri_handler(s_http_server, &index_uri);
    httpd_register_uri_handler(s_http_server, &save_uri);
    ESP_LOGI(TAG, "portal HTTP server started at http://192.168.4.1/");
    return ESP_OK;
}

static void dns_server_task(void *arg)
{
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGW(TAG, "dns captive portal task could not open UDP socket");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in bind_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
    ESP_LOGI(TAG, "dns captive portal responder started on UDP/53");

    while (true) {
        uint8_t buffer[256];
        struct sockaddr_in source_addr = {0};
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&source_addr, &socklen);
        if (len < 12) {
            continue;
        }

        uint8_t response[256] = {0};
        memcpy(response, buffer, (size_t)len);
        response[2] = 0x81;
        response[3] = 0x80;
        response[7] = 0x01;
        response[len++] = 0xC0;
        response[len++] = 0x0C;
        response[len++] = 0x00;
        response[len++] = 0x01;
        response[len++] = 0x00;
        response[len++] = 0x01;
        response[len++] = 0x00;
        response[len++] = 0x00;
        response[len++] = 0x00;
        response[len++] = 0x3C;
        response[len++] = 0x00;
        response[len++] = 0x04;
        response[len++] = 192;
        response[len++] = 168;
        response[len++] = 4;
        response[len++] = 1;
        sendto(sock, response, len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
    }
}

static esp_err_t start_station(void)
{
    if (s_force_ap_only) {
        ESP_LOGW(TAG, "AP-only debug mode enabled, skipping station mode");
        return start_ap_portal();
    }

    wifi_config_t sta_cfg = {0};
    strlcpy((char *)sta_cfg.sta.ssid, s_config.ssid, sizeof(sta_cfg.sta.ssid));
    strlcpy((char *)sta_cfg.sta.password, s_config.password, sizeof(sta_cfg.sta.password));
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta_cfg.sta.pmf_cfg.capable = true;
    sta_cfg.sta.pmf_cfg.required = false;

    ESP_LOGI(TAG, "starting station mode for SSID '%s'", s_config.ssid);
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set sta mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg), TAG, "set sta config failed");
    if (!s_wifi_started) {
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");
        s_wifi_started = true;
    }
    s_portal_active = false;
    s_sta_retry_count = 0;

    app_event_t event = {
        .type = APP_EVENT_WIFI_DISCONNECTED,
        .data.wifi = {.reconnecting = true},
    };
    xQueueSend(s_event_queue, &event, 0);
    return ESP_OK;
}

static esp_err_t start_ap_portal(void)
{
    if (s_portal_active) {
        return ESP_OK;
    }

    wifi_config_t ap_cfg = {0};
    strlcpy((char *)ap_cfg.ap.ssid, s_config.ap_ssid, sizeof(ap_cfg.ap.ssid));
    strlcpy((char *)ap_cfg.ap.password, s_config.ap_password, sizeof(ap_cfg.ap.password));
    ap_cfg.ap.ssid_len = (uint8_t)strlen(s_config.ap_ssid);
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode = strlen(s_config.ap_password) > 0U ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_LOGI(TAG,
             "starting captive portal hotspot: ssid='%s' auth=%s ip=http://192.168.4.1/",
             s_config.ap_ssid,
             strlen(s_config.ap_password) > 0U ? "WPA/WPA2-PSK" : "open");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "set apsta failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg), TAG, "set ap config failed");
    if (!s_wifi_started) {
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");
        s_wifi_started = true;
    }
    ESP_RETURN_ON_ERROR(start_http_server(), TAG, "portal http failed");

    if (s_dns_task == NULL) {
        xTaskCreate(dns_server_task, "dns_server", 3072, NULL, 4, &s_dns_task);
    }
    s_portal_active = true;

    app_event_t event = {.type = APP_EVENT_WIFI_PORTAL_STARTED};
    xQueueSend(s_event_queue, &event, 0);
    return ESP_OK;
}

esp_err_t wifi_portal_start(QueueHandle_t event_queue, const app_config_t *config)
{
    if ((event_queue == NULL) || (config == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    s_event_queue = event_queue;
    s_config = *config;

    ESP_LOGI(TAG, "initializing Wi-Fi and captive portal manager");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop create failed");
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_init_cfg), TAG, "wifi init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL), TAG, "wifi handler register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL), TAG, "ip handler register failed");

    if (s_force_ap_only) {
        ESP_LOGW(TAG, "AP-only debug mode enabled, starting captive portal hotspot unconditionally");
        return start_ap_portal();
    }

    if (strlen(config->ssid) > 0U) {
        ESP_LOGI(TAG, "saved Wi-Fi credentials found, trying station mode first");
        esp_err_t sta_err = start_station();
        if (sta_err == ESP_OK) {
            return ESP_OK;
        }
    }

    ESP_LOGI(TAG, "no saved Wi-Fi credentials found, starting captive portal hotspot immediately");
    return start_ap_portal();
}

esp_err_t wifi_portal_force_config_mode(void)
{
    return start_ap_portal();
}
