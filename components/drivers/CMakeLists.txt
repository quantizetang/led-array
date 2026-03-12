idf_component_register(SRCS "mpu6050_driver.c" "wifi_portal.c" "ws2812_matrix.c"
    INCLUDE_DIRS "include"
    REQUIRES app_core config_store driver esp_http_server esp_wifi esp_event esp_netif nvs_flash lwip)
