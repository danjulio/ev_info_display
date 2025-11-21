#include "Buzzer.h"
#include "esp_log.h"
#include "I2C_Driver.h"
#include "TCA9554PWR.h"


static const char* TAG = "TCA9554PWR";


static uint8_t Read_REG(uint8_t REG, uint8_t* Data);
static esp_err_t Write_REG(uint8_t REG, uint8_t Data);



/*****************************************************  Initialize TCA9954PWR   ****************************************************/ 
esp_err_t EXIO_Init()
{
	esp_err_t ret;
	
	ESP_LOGI(TAG, "Init EXIO");
    if ((ret = Write_REG(TCA9554_CONFIG_REG, 0x00)) != ESP_OK) {
    	ESP_LOGE(TAG, "Config Mode failed - %d", ret);
    } else {
    	if ((ret = Set_EXIO(TCA9554_EXIO8, false)) != ESP_OK) {
    		ESP_LOGE(TAG, "Buzzer off write failed - %d", ret);
    	}
    }
    
    return ret;
}


/********************************************************** Set EXIO mode **********************************************************/       
esp_err_t Mode_EXIO(uint8_t Pin,uint8_t State)   
{
	esp_err_t ret;
	uint8_t bitsStatus;
	uint8_t Data;
	
    ret = Read_REG(TCA9554_CONFIG_REG, &bitsStatus);
    if (ret == ESP_OK) {                        
    	Data = (0x01 << (Pin-1)) | bitsStatus;  
    	ret = Write_REG(TCA9554_CONFIG_REG, Data);
    }
    return ret;
}
esp_err_t Mode_EXIOS(uint8_t PinState)
{
	esp_err_t ret;
	
    ret = Write_REG(TCA9554_CONFIG_REG, PinState);
    return ret;
}


/********************************************************** Read EXIO status **********************************************************/       
esp_err_t Read_EXIO(uint8_t Pin, uint8_t* data)
{
	esp_err_t ret;
	uint8_t inputBits = 0;
	
    ret = Read_REG(TCA9554_INPUT_REG, &inputBits);                                  
    *data = (inputBits >> (Pin-1)) & 0x01;                             
    return ret;                                                              
}
esp_err_t Read_EXIOS(uint8_t* data)
{
	esp_err_t ret;
	
	ret = Read_REG(TCA9554_INPUT_REG, data);
	return ret;                                                                    
}


/********************************************************** Set the EXIO output status **********************************************************/  
esp_err_t Set_EXIO(uint8_t Pin,uint8_t State)
{
	esp_err_t ret = ESP_ERR_INVALID_ARG;
    uint8_t Data = 0;
    uint8_t bitsStatus;
    
    if (State < 2 && Pin < 9 && Pin > 0){     
    	ret = Read_REG(TCA9554_OUTPUT_REG, &bitsStatus);
    	if (ret == ESP_OK) {
        	if (State == 1)                                     
            	Data = (0x01 << (Pin-1)) | bitsStatus;                 
        	else if(State == 0) 
            	Data = (~(0x01 << (Pin-1)) & bitsStatus);  
        	ret = Write_REG(TCA9554_OUTPUT_REG,Data);
        }
    }
    else {                                                                      
        ESP_LOGE(TAG, "Parameter error, please enter the correct parameter!");
    }
	return ret;
}
esp_err_t Set_EXIOS(uint8_t PinState)
{
    return Write_REG(TCA9554_OUTPUT_REG, PinState);                                            
}


/********************************************************** Flip EXIO state **********************************************************/  
esp_err_t Set_Toggle(uint8_t Pin)
{
	esp_err_t ret;
    uint8_t bitsStatus;
    
    ret = Read_EXIO(Pin, &bitsStatus);
    if (ret == ESP_OK) {                                          
    	ret = Set_EXIO(Pin, (bool)!bitsStatus);
    }
    
    return ret;
}




/*****************************************************  Operation register REG   ****************************************************/   
static uint8_t Read_REG(uint8_t REG, uint8_t* Data)
{
	esp_err_t ret; 
    
    ret = I2C_Read(TCA9554_ADDRESS, REG, Data, 1);                                                       
	                                                       
    return ret;                                                                
}

static esp_err_t Write_REG(uint8_t REG, uint8_t Data)
{
	return I2C_Write(TCA9554_ADDRESS, REG, &Data, 1);                                                      
}