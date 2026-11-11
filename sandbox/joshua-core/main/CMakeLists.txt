# Pick the lesson to build by changing this one line, then run: idf.py build
set(LESSON "01_shared_counter_race.c")

idf_component_register(SRCS "05_rtc_clock.c" "${LESSON}" INCLUDE_DIRS "." REQUIRES core nvs_flash)
