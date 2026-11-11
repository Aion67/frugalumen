# Pick the lesson to build by changing this one line, then run: idf.py build
set(LESSON "01_read_pir.c")

idf_component_register(SRCS "05_full_module.c" "${LESSON}" INCLUDE_DIRS "." REQUIRES core lighting esp_driver_gpio)

