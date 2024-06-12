//guard against multiple inclusions
#ifndef _MODBUS_MULTI_H
#define _MODBUS_MULTI_H

#include "Arduino.h"
#include <LinkedList.h>
#include "ModbusMasterFork.h"

enum modbusDataType{
    INT16,
    UINT16,
    DWORD,
    INT32,
    UINT32,
    //INT64,
    //UINT64,
    FLOAT,
    //DOUBLE,
    REAL4
};



class modbusVariable{
    public:
        modbusVariable(uint16_t , modbusDataType, const char* );
        ~modbusVariable();
        const char *getName(void);
        uint16_t getRegisterAddress(void);
        uint8_t readVar(bool updateStatus);
        String getFormatedData(void);
        void    setModbusHandle(ModbusMaster *);
        modbusDataType getDataType(void);
        bool isMeasurementValid(void);
        float getAsFloat(void);
        uint32_t getAsUint(void);

    private:
        void *_data;
        uint16_t       _address;
        char           *_name;
        modbusDataType _varDataType;
        ModbusMaster    *_modbusReader;
        uint8_t        readReal(void);
        uint8_t        readDWORD(void);
		uint8_t 		readSHORT(void);

        bool            _lastReadSuccess;
};

class modbusDevice{
    public:
        modbusDevice(uint8_t , const char* );
        bool addVar(uint16_t , modbusDataType, const char* );
        uint8_t getAddress(void);
        const char *getName(void);
        LinkedList<modbusVariable*> _varList;   //To-Do: Make private
    private:
        uint8_t _address;
        char *_name;
};

#endif
