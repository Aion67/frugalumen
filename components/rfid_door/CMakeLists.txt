idf_component_register(SRCS "rfid_door.c" "mfrc522.c" "door_servo.c" "buzzer.c"
                       INCLUDE_DIRS "include"
                       REQUIRES core esp_driver_gpio esp_driver_spi esp_driver_ledc)
