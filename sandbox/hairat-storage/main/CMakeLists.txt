# Pick the lesson to build by changing this one line, then run: idf.py build
set(LESSON "01_mount_sd.c")

idf_component_register(SRCS "main.c" "03_csv_events.c" "04_full_module.c" "${LESSON}" INCLUDE_DIRS "."
                       REQUIRES core storage fatfs sdmmc esp_driver_sdspi esp_driver_spi)
pi)

pi)
