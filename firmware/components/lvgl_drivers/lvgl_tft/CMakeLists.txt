file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../../platform/EXIO ../../lvgl
                       REQUIRES esp_driver_ledc esp_lcd lvgl lvgl_tft)