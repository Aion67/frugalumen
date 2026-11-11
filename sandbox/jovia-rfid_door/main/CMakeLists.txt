# Pick the lesson to build by changing this one line, then run: idf.py build
set(LESSON "01_spi_read_version.c")

idf_component_register(SRCS "main.c" "05_full_module.c" "${LESSON}" INCLUDE_DIRS "."
                       REQUIRES core rfid_door esp_driver_gpio esp_driver_spi esp_driver_ledc)

