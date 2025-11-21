#ifndef TCA9554PRW_H_
#define TCA9554PRW_H_

#include "esp_system.h"
#include <stdio.h>


#define TCA9554_EXIO1 0x01
#define TCA9554_EXIO2 0x02
#define TCA9554_EXIO3 0x03
#define TCA9554_EXIO4 0x04
#define TCA9554_EXIO5 0x05
#define TCA9554_EXIO6 0x06
#define TCA9554_EXIO7 0x07
#define TCA9554_EXIO8 0x08

/****************************************************** The macro defines the TCA9554PWR information ******************************************************/ 

#define TCA9554_ADDRESS             0x20                    // TCA9554PWR I2C address
// TCA9554PWR registers
#define TCA9554_INPUT_REG           0x00                    // Input register,input level
#define TCA9554_OUTPUT_REG          0x01                    // Output register, high and low level output 
#define TCA9554_Polarity_REG        0x02                    // The Polarity Inversion register (register 2) allows polarity inversion of pins defined as inputs by the Configuration register.  
#define TCA9554_CONFIG_REG          0x03                    // Configuration register, mode configuration

/*****************************************************  Initialize TCA9954PWR   ****************************************************/ 
esp_err_t EXIO_Init();

/*****************************************************  Operation register REG   ****************************************************/   
//uint8_t Read_REG(uint8_t REG);                              // Read the value of the TCA9554PWR register REG
//void Write_REG(uint8_t REG,uint8_t Data);                   // Write Data to the REG register of the TCA9554PWR
/********************************************************** Set EXIO mode **********************************************************/       
esp_err_t Mode_EXIO(uint8_t Pin, uint8_t State);              // Set the mode of the TCA9554PWR Pin. The default is Output mode (output mode or input mode). State: 0= Output mode 1= input mode   
esp_err_t Mode_EXIOS(uint8_t PinState);                       // Set the mode of the 7 pins from the TCA9554PWR with PinState  
/********************************************************** Read EXIO status **********************************************************/       
esp_err_t Read_EXIO(uint8_t Pin, uint8_t* data);              // Read the level of the TCA9554PWR Pin
esp_err_t Read_EXIOS(uint8_t* data);                          // Read the level of all pins of TCA9554PWR, the default read input level state, want to get the current IO output state, pass the parameter TCA9554_OUTPUT_REG, such as Read_EXIOS(TCA9554_OUTPUT_REG);
/********************************************************** Set the EXIO output status **********************************************************/  
esp_err_t Set_EXIO(uint8_t Pin, uint8_t State);               // Sets the level state of the Pin without affecting the other pins
esp_err_t Set_EXIOS(uint8_t PinState);                        // Set 7 pins to the PinState state such as :PinState=0x23, 0010 0011 state (the highest bit is not used)
/********************************************************** Flip EXIO state **********************************************************/  
esp_err_t Set_Toggle(uint8_t Pin);                            // Flip the level of the TCA9554PWR Pin

#endif /* TCA9554PRW_H_ */
