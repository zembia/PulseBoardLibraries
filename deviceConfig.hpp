#ifndef _DEVICE_CONFIGS_HPP
#define _DEVICE_CONFIGS_HPP

#include <Preferences.h>
#include <Wire.h>
#include <PCA9535PW.h>



enum pulseIoType {
    pulseInput,
    pulseOutput
};

enum class POST_METHOD:uint8_t {
    NONE,
    HTTP_POST,
    MQTT
};

enum pulseFilterType{
    FILTER_50,
    FILTER_60,
    FILTER_100,
    FILTER_2000,
    NO_FILTER
};

class deviceConfig{
    public:
        deviceConfig(void);
        void setCredentials(String user, String password);
        void configMqtt(String mqttUrl,String mqttTopic, uint16_t mqttPort);
        void configHttp(String httpUrl);
        void setSamplePeriod(uint16_t);
        const char* getUrl(void);
        const char* getUser(void);
        const char* getPassword(void);
        uint16_t getPort(void);
        const char* getTopic(void);
        uint16_t getSamplePeriod(void);
        const char* getAPN(void);
        const char* getApnPassword(void);
        const char* getApnUser(void);
        uint8_t getApnAuth(void);
        void configAPN(String APN,String user, String Password, uint8_t ApnAuth);
        void setPostMethod(POST_METHOD method);
        POST_METHOD getCurrentMethod(void);
        void setCustomName(String name);
        const char* getCustomName(void);
        const char* getCodigoObraDGA(void);
        bool        getDGAenabled(void);
        void        setDGAconfig(String, uint16_t);
        uint16_t    getDGASampleRate(void);

    private:
        String      _user;
        String      _password;
        Preferences _preferences;
        uint16_t    _samplePeriod;
        String      _mqttUrl;
        String      _httpUrl;
        String      _mqttTopic;
        String      _apn;
        String      _apn_user;
        String      _apn_password;
        String      _customName;
        uint8_t     _apnAuthMethod;
        uint16_t    _mqttPort;
        POST_METHOD _postMethod;
        bool        _DGAenable;
        String      _dgaCodigoObra;
        uint16_t    _dgaSampleRate;

        void        _refreshCredentials(void);
        void        _refreshMqtt(void);
        void        _refreshHttp(void);
        void        _refreshGeneralConfig(void);
        void        _refreshApnConfig(void);
};


class pulseIoPort{
    public:
        pulseIoPort(uint32_t port, pulseIoType portType,PCAL9535A::PCAL9535A<TwoWire> &gpio,SemaphoreHandle_t &i2cMutex );
        pulseIoType getType(void);
        const char *getName(void);
        void setPortValue(bool inputData);
        bool getPortValue(void);     
        enum NAVIGATION {
            RIGHT = 0, 
            UP = 1, 
            LEFT = 2, 
            DOWN = 3
        };
    private:
        pulseIoType _varDataType;
        uint32_t _port;
        PCAL9535A::PCAL9535A<TwoWire> *_gpio;
        SemaphoreHandle_t *_i2cMutex;
};



//These ports are i2c
#define AI_SAMPLE_PERIOD 100
class pulseAiPort{
    public:
        pulseAiPort(const char *name, float gain, float offset,bool requirePower,uint8_t address,SemaphoreHandle_t &i2cMutex ,TwoWire &port=Wire );
        float getConvertedData(void);
        float getRawData(void);
        bool  getRequirePower(void);
    private:
        float               _gain;
        float               _offset;
        bool                _requirePower;
        TwoWire             *_i2cPort;
        SemaphoreHandle_t   *_i2cMutex;
        char                *_name;
        uint8_t             _address;
        float               _rawData;
        float               _rawDataFiltered;
        static void         startTaskImpl(void* );
        void                _sampleMethod(void);
        float               _readSensor(void);
        TaskHandle_t        _samplerHandle;
        
};

#endif