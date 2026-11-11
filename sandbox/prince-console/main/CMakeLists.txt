# Pick the lesson to build by changing this one line, then run: idf.py build
set(LESSON "01_echo_uart.c")

idf_component_register(SRCS "${LESSON}" INCLUDE_DIRS "."
                       REQUIRES core console storage nvs_flash esp_driver_uart)
