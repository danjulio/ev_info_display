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
#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

#include "esp_system.h"
#include <stdint.h>



//
// Configuration
//
#define I2C_SCL_IO                  7         /*!< GPIO number used for I2C master clock */
#define I2C_SDA_IO                  15         /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0         /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          100000    /*!< I2C master clock frequency */
#define I2C_MASTER_TIMEOUT_MS       1000


//
// I2C API
//
esp_err_t I2C_Init(void);
esp_err_t I2C_Write(uint8_t Driver_addr, uint8_t Reg_addr, const uint8_t *Reg_data, uint32_t Length);
esp_err_t I2C_WriteReg16(uint8_t Driver_addr, uint16_t Reg_addr, const uint8_t *Reg_data, uint32_t Length);
esp_err_t I2C_Read(uint8_t Driver_addr, uint8_t Reg_addr, uint8_t *Reg_data, uint32_t Length);
esp_err_t I2C_ReadReg16(uint8_t Driver_addr, uint16_t Reg_addr, uint8_t *Reg_data, uint32_t Length);

#endif /* I2C_DRIVER_H */
