#ifndef ST7701S_H_
#define ST7701S_H_

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "hal/lcd_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "lvgl.h"
#include "TCA9554PWR.h"


#define SPI_METHOD        1
#define IOEXPANDER_METHOD 0


/********************* LCD *********************/

#define LCD_MOSI               1
#define LCD_SCLK               2
#define LCD_CS                 -1      // Using EXIO
// The pixel number in horizontal and vertical
#define LCD_H_RES              480
#define LCD_V_RES              480

#define LCD_PIXEL_CLOCK_HZ     (18 * 1000 * 1000)

#define PIN_NUM_BK_LIGHT       6
#define PIN_NUM_HSYNC          38
#define PIN_NUM_VSYNC          39
#define PIN_NUM_DE             40
#define PIN_NUM_PCLK           41
#define PIN_NUM_DATA0          5  // B0
#define PIN_NUM_DATA1          45 // B1
#define PIN_NUM_DATA2          48 // B2
#define PIN_NUM_DATA3          47 // B3
#define PIN_NUM_DATA4          21 // B4
#define PIN_NUM_DATA5          14 // G0
#define PIN_NUM_DATA6          13 // G1
#define PIN_NUM_DATA7          12 // G2
#define PIN_NUM_DATA8          11 // G3
#define PIN_NUM_DATA9          10 // G4
#define PIN_NUM_DATA10         9  // G5
#define PIN_NUM_DATA11         46 // R0
#define PIN_NUM_DATA12         3  // R1
#define PIN_NUM_DATA13         8  // R2
#define PIN_NUM_DATA14         18 // R3
#define PIN_NUM_DATA15         17 // R4
#define PIN_NUM_DISP_EN        -1

#if CONFIG_DOUBLE_FB
#define LCD_NUM_FB             2
#else
#define LCD_NUM_FB             1
#endif 



#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_LS_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_HS_CH0_GPIO       PIN_NUM_BK_LIGHT
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0
#define LEDC_TEST_DUTY         (4000)
#define LEDC_ResolutionRatio   LEDC_TIMER_13_BIT
#define LEDC_MAX_Duty          ((1 << LEDC_ResolutionRatio) - 1)
#define Backlight_MAX          100      


#if CONFIG_AVOID_TEAR_EFFECT_WITH_SEM
extern SemaphoreHandle_t sem_vsync_end;
extern SemaphoreHandle_t sem_gui_ready;
#endif


extern esp_lcd_panel_handle_t panel_handle;

typedef struct{
    char method_select;
    //SPI config_t
    spi_device_handle_t spi_device;
    spi_bus_config_t spi_io_config_t;
    spi_device_interface_config_t st7701s_protocol_config_t;
}ST7701S;

typedef ST7701S * ST7701S_handle;

ST7701S_handle ST7701S_newObject(int SDA, int SCL, int CS, char channel_select, char method_select);//Create new object
void ST7701S_screen_init(ST7701S_handle St7701S_handle, unsigned char type);//Screen initialization
void ST7701S_delObject(ST7701S_handle St7701S_handle);//Delete object
void ST7701S_WriteCommand(ST7701S_handle St7701S_handle, uint8_t cmd);//SPI write instruction
void ST7701S_WriteData(ST7701S_handle St7701S_handle, uint8_t data);//SPI write data
esp_err_t ST7701S_CS_EN(void);//Enables SPI CS
esp_err_t ST7701S_CS_Dis(void);//Disable SPI CS
esp_err_t ST7701S_reset(void);// LCD Reset


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LCD_Init(lv_disp_drv_t* disp_drv);
void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);

/********************* BackLight *********************/
void Backlight_Init(void);
void Set_Backlight(uint8_t Light);
uint8_t Get_Backlight();

#endif /* ST7701S_H_ */