file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS . ../components/can ../components/gui ../components/lvgl ../components/lvgl_drivers ../components/platform ../components/utilities ../components/vehicle)