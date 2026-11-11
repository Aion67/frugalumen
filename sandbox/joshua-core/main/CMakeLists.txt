# Pick the lesson to build by changing this one line, then run: idf.py build
set(LESSON "01_shared_counter_race.c")

idf_component_register(SRCS "06_full_core.c" "02_mutex_fix.c" "05_rtc_clock.c" "${LESSON}" INCLUDE_DIRS "." REQUIRES core nvs_flash)
h)
nvs_flash)
h)
nvs_flash)
