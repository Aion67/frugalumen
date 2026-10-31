idf_component_register(SRCS "console.c" "auth.c"
                       INCLUDE_DIRS "include"
                       REQUIRES core storage nvs_flash esp_driver_uart)
