idf_component_register(SRCS "display.c" "lcd1602.c"
                       INCLUDE_DIRS "include"
                       REQUIRES core esp_driver_i2c)
