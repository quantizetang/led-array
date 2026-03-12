idf_component_register(SRCS "app_main.c"
    REQUIRES app_core config_store drivers effects providers nvs_flash esp_timer)
