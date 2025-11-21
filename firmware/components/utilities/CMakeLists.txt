file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../can
                       REQUIRES bt esp_app_format esp_netif esp_wifi nvs_flash)

