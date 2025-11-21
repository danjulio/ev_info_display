file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../can ../data_broker
                       REQUIRES can data_broker esp_timer)