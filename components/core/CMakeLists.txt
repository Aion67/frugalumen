idf_component_register(SRCS "core.c" "rtc_ds1307.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos nvs_flash esp_driver_gpio esp_driver_spi esp_driver_i2c)
