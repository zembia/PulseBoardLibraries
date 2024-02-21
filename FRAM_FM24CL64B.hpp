#ifndef _FRAM_FM24_H
#define _FRAM_FM24_H

#include <Arduino.h>
#include <Wire.h>

#define FRAM_DEFAULT_ADDR 0x50
#define MAX_ADDRESS 8192

class FRAM_FM24{
    public:
        FRAM_FM24(SemaphoreHandle_t &i2cMutex,TwoWire &port = Wire, uint8_t address=FRAM_DEFAULT_ADDR);
        void writeArray(uint16_t address, uint16_t size, uint8_t *data);
        void readArray(uint16_t address, uint16_t size, uint8_t *data);
        bool selfTest(void);
    private:
        uint8_t _i2cAddr;
        TwoWire *_i2cPort;
        SemaphoreHandle_t *_i2cMutex;
};



#endif