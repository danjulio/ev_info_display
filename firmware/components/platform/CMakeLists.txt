file(GLOB SOURCES Buzzer/*.c EXIO/*.c I2C_Driver/*.c QMI8658/*.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS Buzzer EXIO I2C_Driver QMI8658
                       REQUIRES driver)
