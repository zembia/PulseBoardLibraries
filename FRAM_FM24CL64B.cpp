#include "FRAM_FM24CL64B.hpp"

FRAM_FM24::FRAM_FM24(SemaphoreHandle_t &i2cMutex,TwoWire &port,uint8_t address)
{
    _i2cAddr = address;
    _i2cPort =&port;
    _i2cMutex = &i2cMutex;
    
}

void FRAM_FM24::writeArray(uint16_t address,uint16_t size, uint8_t *data)
{
    //input validation
    if (size == 0)
    {
        ESP_LOGE("FRAM", "SIZE IS 0");
        return;
    }
    if (data == NULL)
    {
        ESP_LOGE("FRAM", "DATA POINTER IS NULL");
        return;
    }
    //data too big
    if (address+(uint16_t)size>=MAX_ADDRESS)
    {
        ESP_LOGE("FRAM", "ADDRESS IS TO BIG");
        return;
    }

    if (*_i2cMutex != NULL && (xSemaphoreTake(*_i2cMutex, (TickType_t ) 10) == pdTRUE) )
    {          
        _i2cPort->beginTransmission(_i2cAddr);
        _i2cPort->write((address>>8) &0xFF);
        _i2cPort->write((address) &0xFF);
        for (int i=0;i<size;i++)
        {
            _i2cPort->write(data[i]);
        }
        _i2cPort->endTransmission();
        
        xSemaphoreGive(*_i2cMutex);

    }
    else
    {
        ESP_LOGE("FRAM", "Could not take semaphore");
    }

}

void FRAM_FM24::readArray(uint16_t address,uint16_t size, uint8_t *data)
{
    //input validation
    if (size == 0)
    {
        ESP_LOGE("FRAM", "SIZE IS 0");
        return;
    }
    if (data == NULL)
    {
        ESP_LOGE("FRAM", "DATA POINTER IS NULL");
        return;
    }
    //data too big
    if (address+(uint16_t)size>=MAX_ADDRESS)
    {
        ESP_LOGE("FRAM", "ADDRESS IS TO BIG");
        return;
    }

    if (*_i2cMutex != NULL && (xSemaphoreTake(*_i2cMutex, (TickType_t ) 10) == pdTRUE) )
    {      
        _i2cPort->beginTransmission(_i2cAddr);
        _i2cPort->write((address>>8) &0xFF);
        _i2cPort->write((address) &0xFF);
        _i2cPort->endTransmission(false);
        _i2cPort->requestFrom(_i2cAddr,size,true);


        for (int i=0;i<size;i++)
        {
            data[i]=_i2cPort->read(); //maybe readbytes is faster? 
        }
        
        xSemaphoreGive(*_i2cMutex);
    }
    else
    {
        ESP_LOGE("FRAM", "Could not take semaphore");
    }
}

#define LEN_TO_TEST 64
bool FRAM_FM24::selfTest(void)
{
    uint8_t dataToWrite[LEN_TO_TEST];
    uint8_t dataToRead[LEN_TO_TEST];
    for (uint16_t i=0;i<LEN_TO_TEST;i++)
    {
        dataToWrite[i]=i;
    }

    this->writeArray(32,LEN_TO_TEST,dataToWrite);
    this->readArray(32,LEN_TO_TEST,dataToRead);
    for (uint16_t i=0;i<LEN_TO_TEST;i++)
    {
        //Serial.printf("DataRead: %X\t DataWriten: %X\r\n",dataToRead[i],dataToWrite[i]);
        if (dataToWrite[i] != dataToRead[i])
        {
            return false;
        }
    }
    return true;
}