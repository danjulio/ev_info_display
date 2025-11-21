file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../utilities ../vehicle
                       REQUIRES esp_driver_twai esp_netif esp_timer)