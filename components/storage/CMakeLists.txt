idf_component_register(SRCS "storage.c"
                       INCLUDE_DIRS "include"
                       REQUIRES core fatfs sdmmc esp_driver_sdspi)
