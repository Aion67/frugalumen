# Pick the lesson to build by changing this one line, then run: idf.py build
set(LESSON "01_mount_sd.c")

idf_component_register(SRCS "${LESSON}" INCLUDE_DIRS "."
                       REQUIRES core storage fatfs sdmmc esp_driver_sdspi esp_driver_spi)
