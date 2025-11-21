/**
 * @file touch_driver.c
 */
#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_driver.h"
#include "TCA9554PWR.h"


void touch_driver_init(void)
{
	esp_err_t ret;
	
	// Setup ESP32 TP INT pin as input with pull-up or pull-down based on the GT911 I2C address we want
	// (TP_INT sampled during reset)
	gpio_set_direction(TP_INT, GPIO_MODE_INPUT);
	if (GT911_I2C_SLAVE_ADDR == 0x14) {
		gpio_set_pull_mode(TP_INT, GPIO_PULLUP_ONLY);
	} else {
		gpio_set_pull_mode(TP_INT, GPIO_PULLDOWN_ONLY);
	}
	
	// Hardware reset
    ret = Set_EXIO(TCA9554_EXIO2,false);
    vTaskDelay(pdMS_TO_TICKS(10));
    ret |= Set_EXIO(TCA9554_EXIO2,true);
    vTaskDelay(pdMS_TO_TICKS(150));
	
	// Touch controller initialization
	if (ret == ESP_OK) {
    	gt911_init(GT911_I2C_SLAVE_ADDR);
    }
}

#if LVGL_VERSION_MAJOR >= 8
void touch_driver_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
#else
bool touch_driver_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
#endif
{
    bool res = false;

    res = gt911_read(drv, data);

#if LVGL_VERSION_MAJOR >= 8
    data->continue_reading = res;
#else
    return res;
#endif
}

