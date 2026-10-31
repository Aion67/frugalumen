# Pick the lesson to build by changing this one line, then run: idf.py build
set(LESSON "01_i2c_scan.c")

idf_component_register(SRCS "${LESSON}" INCLUDE_DIRS "." REQUIRES core display esp_driver_i2c)
