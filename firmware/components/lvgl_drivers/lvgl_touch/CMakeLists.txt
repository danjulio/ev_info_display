file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS . ../../platform/EXIO ../../platform/I2C_Driver
                       REQUIRES lvgl)
