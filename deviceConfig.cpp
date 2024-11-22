#include "deviceConfig.hpp"


deviceConfig::deviceConfig(void)
{

    _DGAenable = false;
    //At the constructor we get the credentials from NV memory
    _refreshCredentials();

    //Now we get mqtt Preferences
    _refreshMqtt();
    _refreshHttp();

    //We refresh other general preferences
    _refreshGeneralConfig();

    //We refresh APN configurations
    _refreshApnConfig();

}

void deviceConfig::_refreshCredentials(void)
{
    _preferences.begin("credentials",false); 
    _user = _preferences.getString("user",""); //default value is ""
    _password = _preferences.getString("password",""); //default value is ""
    _preferences.end();
}

void deviceConfig::_refreshApnConfig(void)
{
    _preferences.begin("simconfig",false);
    _apn = _preferences.getString("apn","data.mono");
    _apn_user = _preferences.getString("user","");
    _apn_password = _preferences.getString("password","");
    _apnAuthMethod = _preferences.getShort("auth",1);
    _preferences.end();
}

const char * deviceConfig::getAPN(void)
{
    return _apn.c_str();
}

const char *deviceConfig::getApnPassword(void)
{
    return _apn_password.c_str();
}

const char *deviceConfig::getApnUser(void)
{
    return _apn_user.c_str();
}
uint8_t deviceConfig::getApnAuth(void)
{
    return _apnAuthMethod;
}

void deviceConfig::configAPN(String APN,String user, String Password, uint8_t ApnAuth)
{
    ESP_LOGI("configAPN","New configuration. APN: %s",APN.c_str());
    _preferences.begin("simconfig",false);
    _preferences.putString("apn",APN);
    _preferences.putString("user",user);
    _preferences.putString("password",Password);
    _preferences.putShort("auth",ApnAuth);
    _preferences.end();
    _refreshApnConfig();
}


void deviceConfig::configHttp(String httpUrl)
{
    _preferences.begin("http",false);
    _preferences.putString("url",httpUrl);
    _preferences.end();
    _refreshHttp();
}
void deviceConfig::configMqtt(String mqttUrl,String mqttTopic, uint16_t mqttPort, String mqttTopicRx)
{
    _preferences.begin("mqtt",false);
    _preferences.putUShort("port",mqttPort);
    _preferences.putString("topic",mqttTopic);
    _preferences.putString("topicRx",mqttTopicRx);
    _preferences.putString("url",mqttUrl);
    _preferences.end();
    _refreshMqtt();
}

const char *deviceConfig::getUrl(void)
{
    if (_postMethod == POST_METHOD::HTTP_POST)
    {
        return _httpUrl.c_str();
    }
    if (_postMethod == POST_METHOD::MQTT)
    {
        return _mqttUrl.c_str();
    }
    else
    {
        return "";
    }
    
}
uint16_t deviceConfig::getPort(void)
{
    return _mqttPort;
}
const char * deviceConfig::getTopic(void)
{
    return _mqttTopic.c_str();
}
const char * deviceConfig::getTopicRx(void)
{
    return _mqttTopicRx.c_str();
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
    _mqttTopicRx = _preferences.getString("topicRx","");
    _mqttUrl    = _preferences.getString("url","");
    _preferences.end();
}

void deviceConfig::_refreshHttp(void)
{
    _preferences.begin("http",false); 
    _httpUrl    = _preferences.getString("url","");
    _preferences.end();
}

void deviceConfig::setPostMethod(POST_METHOD method)
{
    _preferences.begin("generalConfig",false);
    _preferences.putUChar("postMethod",(uint8_t)(method));
    _preferences.end();
    _refreshGeneralConfig();
}

POST_METHOD deviceConfig::getCurrentMethod(void)
{
    return _postMethod;
}
void deviceConfig::_refreshGeneralConfig(void)
{
    _preferences.begin("generalConfig",false);
    _samplePeriod = _preferences.getUShort("samplePeriod",60);
    _postMethod = (POST_METHOD)_preferences.getUChar("postMethod",(uint8_t)(POST_METHOD::NONE));
    _customName = _preferences.getString("name","pulse board");
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
    _refreshGeneralConfig();
}
const char* deviceConfig::getCustomName(void)
{
    return _customName.c_str();
}
void deviceConfig::setCustomName(String name)
{
    if (name.length()>0)
    {
        _preferences.begin("generalConfig",false);
        _preferences.putString("name",name);
        _preferences.end(); //close preferences
        _refreshGeneralConfig();
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


void deviceConfig::setDGAconfig(String codigoObra, uint16_t sampleRate)
{
    _dgaSampleRate = sampleRate;
    _dgaCodigoObra = codigoObra;
    _DGAenable = true;
}

uint16_t deviceConfig::getDGASampleRate(void)
{
    return _dgaSampleRate;
}


const char *deviceConfig::getCodigoObraDGA(void)
{
    return _dgaCodigoObra.c_str();
}

bool deviceConfig::getDGAenabled(void)
{
    return _DGAenable;
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
        if (xSemaphoreTake(*_i2cMutex, (TickType_t ) 100) == pdTRUE) 
        {
            //1 salida, 0 entrada
            _gpio->pinMode(_port,0);
            xSemaphoreGive(*_i2cMutex);
        }
    }
    else
    {
        if (xSemaphoreTake(*_i2cMutex, (TickType_t ) 100) == pdTRUE) 
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
         if (xSemaphoreTake(*_i2cMutex, (TickType_t ) 100) == pdTRUE) 
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
        if (xSemaphoreTake(*_i2cMutex, (TickType_t ) 100) == pdTRUE) 
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

