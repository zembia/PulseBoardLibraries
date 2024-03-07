#include "deviceConfig.hpp"


deviceConfig::deviceConfig(void)
{
    //At the constructor we get the credentials from NV memory
    _refreshCredentials();

    //Now we get mqtt Preferences
    _refreshMqtt();

    //We refresh other general preferences
    _refreshGeneralConfig();

}

void deviceConfig::_refreshCredentials(void)
{
    _preferences.begin("credentials",false); 
    _user = _preferences.getString("user",""); //default value is ""
    _password = _preferences.getString("password",""); //default value is ""
    _preferences.end();
}

void deviceConfig::configMqtt(String mqttUrl,String mqttTopic, uint16_t mqttPort)
{
    _preferences.begin("mqtt",false);
    _preferences.putUShort("port",mqttPort);
    _preferences.putString("topic",mqttTopic);
    _preferences.putString("url",mqttUrl);
    _preferences.end();
}

const char *deviceConfig::getUrl(void)
{
    return _mqttUrl.c_str();
}
uint16_t deviceConfig::getPort(void)
{
    return _mqttPort;
}
const char * deviceConfig::getTopic(void)
{
    return _mqttTopic.c_str();
}

uint16_t deviceConfig::getSamplePeriod(void)
{
    return _samplePeriod;
}

void deviceConfig::_refreshMqtt(void)
{
    _preferences.begin("mqtt",false); 
    _mqttPort   = _preferences.getUShort("port",1883);
    _mqttTopic  = _preferences.getString("topic","");
    _mqttUrl    = _preferences.getString("url","");
    _preferences.end();
}

void deviceConfig::_refreshGeneralConfig(void)
{
    _preferences.begin("generalConfig",false);
    _samplePeriod = _preferences.getUShort("samplePeriod",60);
    _preferences.end();
}

void deviceConfig::setSamplePeriod(uint16_t samplePeriod)
{
    if (samplePeriod != 0)
    {
        _preferences.begin("generalConfig",false);
        _preferences.putUShort("samplePeriod",samplePeriod);
        _preferences.end();
    }
}
const char *deviceConfig::getUser(void)
{
    return _user.c_str();
}
const char* deviceConfig::getPassword(void)
{
    return _password.c_str();
}
void deviceConfig::setCredentials(String user, String password)
{

    _preferences.begin("credentials",false); //open as read/write
    _preferences.putString("user",user);
    _preferences.putString("password",password);
    _preferences.end(); //close preferences
    _refreshCredentials();
}


/**********************************************************************/
/*                                                                    */
/*                       Digital ports                                */
/*                                                                    */
/**********************************************************************/
pulseIoPort::pulseIoPort(uint32_t portNumber, pulseIoType type,PCAL9535A::PCAL9535A<TwoWire> &gpio,SemaphoreHandle_t &i2cMutex )
{
    _port = portNumber;
    _gpio = &gpio;
    _varDataType = type;
    _i2cMutex = &i2cMutex;    
    if (_varDataType == pulseInput)
    {
        if (xSemaphoreTake(*_i2cMutex, (TickType_t ) 5) == pdTRUE) 
        {
            //1 salida, 0 entrada
            _gpio->pinMode(_port,0);
            xSemaphoreGive(*_i2cMutex);
        }
    }
    else
    {
        if (xSemaphoreTake(*_i2cMutex, (TickType_t ) 5) == pdTRUE) 
        {
            //1 salida, 0 entrada
            _gpio->pinMode(_port,1);
            xSemaphoreGive(*_i2cMutex);
        }
    }
}

bool pulseIoPort::getPortValue(void)
{
    bool readData;
    if(_varDataType == pulseInput)
    {
         if (xSemaphoreTake(*_i2cMutex, (TickType_t ) 5) == pdTRUE) 
        {
            readData = _gpio->digitalRead(_port);
            xSemaphoreGive(*_i2cMutex);
        }
        
    }
    return readData;
    
}

void pulseIoPort::setPortValue(bool inputData)
{
    if (_varDataType == pulseOutput)
    {
        if (xSemaphoreTake(*_i2cMutex, (TickType_t ) 5) == pdTRUE) 
        {
            _gpio->digitalWrite(_port,inputData);
            xSemaphoreGive(*_i2cMutex);
        }
        
    }
    return;
}


/**********************************************************************/
/*                                                                    */
/*                        Analog ports                                */
/*                                                                    */
/**********************************************************************/

pulseAiPort::pulseAiPort(const char *name, float gain, float offset, uint8_t address,SemaphoreHandle_t &i2cMutex, TwoWire &port )
{
    _gain = gain;
    _offset = offset;

    //To-Do: check for _name correct memory allocation;
    if (name != NULL)
    {
        _name = (char*)malloc(strlen(name)*sizeof(char));
    }
    _address = address;
    _i2cMutex = &i2cMutex;
    _i2cPort = &port;

    xTaskCreate(
        this->startTaskImpl,
        "aiTask",
        2048,
        this,
        4,
        &_samplerHandle
    );
   
}

float pulseAiPort::getConvertedData(void)
{
    return _rawData*_gain + _offset;
}

float pulseAiPort::getRawData(void)
{
    return _rawData;
}

void pulseAiPort::startTaskImpl(void *_this)
{
    static_cast<pulseAiPort*>(_this)->_sampleMethod();
}

float pulseAiPort::_readSensor(void)
{
    uint16_t result;
    if (xSemaphoreTake(*_i2cMutex, (TickType_t ) 20) == pdTRUE) 
    {
        _i2cPort->requestFrom(_address,2U);
        if (_i2cPort->available() == 2)
        {
            result = (_i2cPort->read() << 8) | (_i2cPort->read());
        }
        xSemaphoreGive(*_i2cMutex);
        return (result*3300.0/4096/50/2.61);
    }
    else
    {
        return _rawData;
    }
}

void pulseAiPort::_sampleMethod(void)
{
    const TickType_t xFrequency = pdMS_TO_TICKS(AI_SAMPLE_PERIOD);
    TickType_t xLastWakeTime;
    xLastWakeTime =  xTaskGetTickCount();

    while(1)
    {
        _rawData = _readSensor();
        //Periodic excecution
        vTaskDelayUntil(&xLastWakeTime,xFrequency);
    }
}

