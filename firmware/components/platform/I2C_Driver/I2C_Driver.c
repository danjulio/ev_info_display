/*
 * I2C Driver with locking
 *
 * Completely re-written Waveshare demo API but changed to use new I2C driver and
 * provide locking.
 *
 * Copyright 2025 Dan Julio
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
//#include "driver/i2c_master.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "I2C_Driver.h"
#include <string.h>


//
// I2C constants
//
#define MAX_I2C_DATA_LEN 8

#define I2C_MASTER_TX_BUF_DISABLE   0         /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0         /*!< I2C master doesn't need buffer */


//
// I2C variables
//
static const char* TAG = "I2C";

//static i2c_master_bus_handle_t bus_handle;
static SemaphoreHandle_t i2c_mutex;



//
// Forward declarations for internal functions
//
void _i2c_lock();
void _i2c_unlock();
//esp_err_t _i2c_read_slave(uint8_t addr7, uint8_t *data_rd, size_t size);
//esp_err_t _i2c_write_slave(uint8_t addr7, uint8_t *data_wr, size_t size);



//
// API
//
esp_err_t I2C_Init(void)
{
/*
	i2c_master_bus_config_t i2c_mst_config;
	
	ESP_LOGI(TAG, "Init I2C Master");

	// Create a mutex for thread safety
    i2c_mutex = xSemaphoreCreateMutex();
    
    // Configure the I2C bus
    i2c_mst_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port = I2C_MASTER_NUM;
    i2c_mst_config.scl_io_num = I2C_SCL_IO;
    i2c_mst_config.sda_io_num = I2C_SDA_IO;
    i2c_mst_config.glitch_ignore_cnt = 7;
    i2c_mst_config.intr_priority = 0;
    i2c_mst_config.trans_queue_depth = 0;
    i2c_mst_config.flags.enable_internal_pullup = true;
    i2c_mst_config.flags.allow_pd = 0;
    
	return i2c_new_master_bus(&i2c_mst_config, &bus_handle);
*/
	int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    ESP_LOGI(TAG, "Init I2C Master");

	// Create a mutex for thread safety
//    i2c_mutex = xSemaphoreCreateMutex();

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);

}

/*
// Reg addr is 8 bit
esp_err_t I2C_Write(uint8_t Driver_addr, uint8_t Reg_addr, const uint8_t *Reg_data, uint32_t Length)
{
	esp_err_t ret;
    uint8_t buf[MAX_I2C_DATA_LEN+1];
    
    if (Length > MAX_I2C_DATA_LEN) {
    	return ESP_ERR_INVALID_SIZE;
    }
    
    // Copy Reg_data to buf starting at buf[1]
    buf[0] = Reg_addr;
    memcpy(&buf[1], Reg_data, Length);
    
    _i2c_lock();
    
    ret = _i2c_write_slave(Driver_addr, buf, Length + 1);
    
    _i2c_unlock();
    
    return ret;
}



esp_err_t I2C_WriteReg16(uint8_t Driver_addr, uint16_t Reg_addr, const uint8_t *Reg_data, uint32_t Length)
{
	esp_err_t ret;
    uint8_t buf[MAX_I2C_DATA_LEN+2];
    
    if (Length > MAX_I2C_DATA_LEN) {
    	return ESP_ERR_INVALID_SIZE;
    }
    
    // Copy Reg_data to buf starting at buf[1]
    buf[0] = Reg_addr >> 8;
    buf[1] = Reg_addr & 0xFF;
    memcpy(&buf[2], Reg_data, Length);
    
    _i2c_lock();
    
    ret = _i2c_write_slave(Driver_addr, buf, Length + 2);
    
    _i2c_unlock();
    
    return ret;
}


esp_err_t I2C_Read(uint8_t Driver_addr, uint8_t Reg_addr, uint8_t *Reg_data, uint32_t Length)
{
	esp_err_t ret;
	
	_i2c_lock();
	
	// Write address
	ret = _i2c_write_slave(Driver_addr, &Reg_addr, 1);
	
	// Read data
	if (ret == ESP_OK) {
		ret = _i2c_read_slave(Driver_addr, Reg_data, Length);
	}
	
	_i2c_unlock();
    
    return ret;
}


esp_err_t I2C_ReadReg16(uint8_t Driver_addr, uint16_t Reg_addr, uint8_t *Reg_data, uint32_t Length)
{
	esp_err_t ret;
	uint8_t buf[2];
	
	_i2c_lock();
	
	// Write address
	buf[0] = Reg_addr >> 8;
    buf[1] = Reg_addr & 0xFF;
	ret = _i2c_write_slave(Driver_addr, buf, 2);
	
	// Read data
	if (ret == ESP_OK) {
		ret = _i2c_read_slave(Driver_addr, Reg_data, Length);
	}
	
	_i2c_unlock();
    
    return ret;
}
*/
// Reg addr is 8 bit
esp_err_t I2C_Write(uint8_t Driver_addr, uint8_t Reg_addr, const uint8_t *Reg_data, uint32_t Length)
{
    uint8_t buf[MAX_I2C_DATA_LEN+1];
    
    if (Length > MAX_I2C_DATA_LEN) {
    	return ESP_ERR_INVALID_SIZE;
    }
    
    buf[0] = Reg_addr;
    // Copy Reg_data to buf starting at buf[1]
    memcpy(&buf[1], Reg_data, Length);
    return i2c_master_write_to_device(I2C_MASTER_NUM, Driver_addr, buf, Length+1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}


esp_err_t I2C_WriteReg16(uint8_t Driver_addr, uint16_t Reg_addr, const uint8_t *Reg_data, uint32_t Length)
{
	uint8_t buf[MAX_I2C_DATA_LEN+2];
	
	if (Length > MAX_I2C_DATA_LEN) {
    	return ESP_ERR_INVALID_SIZE;
    }
    
    // Copy Reg_data to buf starting at buf[1]
    buf[0] = Reg_addr >> 8;
    buf[1] = Reg_addr & 0xFF;
    memcpy(&buf[2], Reg_data, Length);
    
    return i2c_master_write_to_device(I2C_MASTER_NUM, Driver_addr, buf, Length+2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}


esp_err_t I2C_Read(uint8_t Driver_addr, uint8_t Reg_addr, uint8_t *Reg_data, uint32_t Length)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, Driver_addr, &Reg_addr, 1, Reg_data, Length, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}


esp_err_t I2C_ReadReg16(uint8_t Driver_addr, uint16_t Reg_addr, uint8_t *Reg_data, uint32_t Length)
{
	uint8_t buf[2];
	
	// Write address
	buf[0] = Reg_addr >> 8;
    buf[1] = Reg_addr & 0xFF;
    
    return i2c_master_write_read_device(I2C_MASTER_NUM, Driver_addr, (const uint8_t*) &buf, 2, Reg_data, Length, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}



//
// Internal functions
//

/**
 * i2c master lock
 */
void _i2c_lock()
{
	xSemaphoreTake(i2c_mutex, portMAX_DELAY);
}


/**
 * i2c master unlock
 */
void _i2c_unlock()
{
	xSemaphoreGive(i2c_mutex);
}


/**
 * Read esp-i2c-slave
 *
 * _______________________________________________________________________________________
 * | start | slave_addr + rd_bit +ack | read n-1 bytes + ack | read 1 byte + nack | stop |
 * --------|--------------------------|----------------------|--------------------|------|
 *
 */
/*
esp_err_t _i2c_read_slave(uint8_t addr7, uint8_t *data_rd, size_t size)
{
	i2c_device_config_t dev_cfg;
	i2c_master_dev_handle_t dev_handle;
	
    if (size == 0) {
        return ESP_OK;
    }
    
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = (uint16_t) addr7;
    dev_cfg.scl_speed_hz = I2C_MASTER_FREQ_HZ;
    
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
    	return ret;
    }
    
    ret = i2c_master_receive(dev_handle, data_rd, size, I2C_MASTER_TIMEOUT_MS);
    
    i2c_master_bus_rm_device(dev_handle);
    
    return ret;
}
*/


/**
 * Write esp-i2c-slave
 *
 * ___________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write n bytes + ack  | stop |
 * --------|---------------------------|----------------------|------|
 *
 */
/*
esp_err_t _i2c_write_slave(uint8_t addr7, uint8_t *data_wr, size_t size)
{
    i2c_device_config_t dev_cfg;
	i2c_master_dev_handle_t dev_handle;
	
	dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = (uint16_t) addr7;
    dev_cfg.scl_speed_hz = I2C_MASTER_FREQ_HZ;
    
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
    	return ret;
    }
    
    ret = i2c_master_transmit(dev_handle, data_wr, size, I2C_MASTER_TIMEOUT_MS);
    
    i2c_master_bus_rm_device(dev_handle);
    
    return ret;
}
*/
