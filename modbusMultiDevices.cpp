#include "modbusMultiDevices.hpp"
//constructor for a modbusVariable
modbusVariable::modbusVariable(uint16_t address, modbusDataType varDataType, const char* name)
{   

    //We copy the name
    _name = (char*)malloc(sizeof(char)*strlen(name)+1);
    strcpy(_name,name);
    //We copy the address to private member of function
    _address = address;

    //Depending on the data type, the size requirements are different    
    _varDataType = varDataType;
    switch (_varDataType)
    {
        /*case INT16:
        case UINT16:
            _data = malloc(sizeof(uint16_t));
            break;*/
        case INT32:
        case DWORD:
        case UINT32:
            _data = malloc(sizeof(uint32_t));
            break;
        /*case INT64:
        case UINT64:
            _data = malloc(sizeof(uint64_t));
            break;*/
        case FLOAT:
        case REAL4:
            _data = malloc(sizeof(float));
            break;
        /*case DOUBLE:
            _data = malloc(sizeof(double));
            break;*/
        default:
            ESP_LOGE("MODBUS_MULTI_DEV","DATA TYPE NOT SET");
            break;

    }
    ESP_LOGI("MODBUS_MULTI_DEV","MODBUS VARIABLE CREATED CORRECTLY");
}


void modbusVariable::setModbusHandle(ModbusMaster *modbusReader)
{
    _modbusReader = modbusReader;
}

float modbusVariable::getAsFloat(void)
{
    return *(float*)_data; 
}

bool modbusVariable::isMeasurementValid(void)
{
    return _lastReadSuccess;
}

uint32_t modbusVariable::getAsUint(void)
{
    return *(uint32_t*)_data;
}

uint8_t modbusVariable::readVar(bool data)
{
    uint8_t result = 255;
    switch(_varDataType)
    {
        case INT32:
        case UINT32:
        case DWORD:
            result = readDWORD();
            break;
        case FLOAT:
        case REAL4:
            result = readReal();
            break;
    }
    
    if (result != _modbusReader->ku8MBSuccess)
    {
        if (data==true)
        {
            _lastReadSuccess = false;
        }
    }
    else
    {
        _lastReadSuccess = true;
    }
    return result;

}

String modbusVariable::getFormatedData(void)
{
    char tempCString[20];
    if (_lastReadSuccess == false)
    {
        return String("");
    }
    switch(_varDataType)
    {
        case INT32:
            snprintf(tempCString,20,"%d",*(int32_t*)_data);
            break;
        case DWORD:
        case UINT32:
            snprintf(tempCString,20,"%u",*(uint32_t*)_data);
            break;
        case FLOAT:
        case REAL4:
            snprintf(tempCString,20,"%.2f",*(float*)_data);
            break;        
    }
    return String(tempCString);
}

uint8_t modbusVariable::readReal(void)
{
    if (_data == NULL)    
    {
        
        return -1;
    }
    //there should be a check here, but these funcitions won't be called by public methods

    uint8_t result;
    uint8_t data_size = 2;
    uint16_t buf[data_size] ={0};
    result = _modbusReader->readHoldingRegisters(_address,data_size);
    if (result == _modbusReader->ku8MBSuccess)
    {
        for (uint8_t j=0;j<data_size;j++)
        {
            buf[j] = _modbusReader->getResponseBuffer(j);
        }
        uint16_t temp;
        temp   =    buf[1];
        buf[1] =    buf[0];
        buf[0] =    temp;
        
    }
    memcpy(_data,buf,sizeof(float));

    return result;
}

uint8_t modbusVariable::readDWORD(void)
{
    if (_data == NULL)
    {
        return -1;
    }
    //there should be a check here, but these funcitions won't be called by public methods
    
    uint8_t result;
    uint8_t data_size = 2;
    uint16_t buf[data_size] ={0};
    result = _modbusReader->readHoldingRegisters(_address,data_size);
    if (result == _modbusReader->ku8MBSuccess)
    {
        for (uint8_t j=0;j<data_size;j++)
        {
            buf[j] = _modbusReader->getResponseBuffer(j);
        }
        
    }
    memcpy(_data,buf,sizeof(uint32_t));
    return result;
}


modbusVariable::~modbusVariable(void)
{
    free(_name);
    free(_data);
}


const char *modbusVariable::getName(void)
{
    return _name;
}

uint16_t modbusVariable::getRegisterAddress(void)
{
    return _address;
}

modbusDataType modbusVariable::getDataType(void)
{
    return _varDataType;
}



//constructor for a modbus device
modbusDevice::modbusDevice(uint8_t address, const char* name)
{
    _address = address;
    _name = (char *)malloc(sizeof(char)*(strlen(name)+1));
    strcpy(_name,name);
    _varList = LinkedList<modbusVariable*>();
}

bool modbusDevice::addVar(uint16_t address, modbusDataType varDataType, const char* name)
{
    modbusVariable *currentVariable  = new modbusVariable(address,varDataType,name);
    _varList.add(currentVariable);
}

uint8_t modbusDevice::getAddress(void)
{
    return _address;
}
const char * modbusDevice::getName(void)
{
    return _name;
}



