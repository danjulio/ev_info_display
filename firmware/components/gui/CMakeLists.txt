file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../can ../data_broker ../lvgl_drivers/lvgl_tft ../../main ../platform/Buzzer ../utilities ../vehicle
                       REQUIRES esp_app_format esp_timer lvgl)