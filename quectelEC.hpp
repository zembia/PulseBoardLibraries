#ifndef _QUECTEL_EC_HPP
#define _QUECTEL_EC_HPP

#include <Wire.h>
#include <PCA9535PW.h>
#include <deviceConfig.hpp>

#define GPRS_Serial Serial1
#define GPRS_SerialNumber 1
#define GPRS_TX 32
#define GPRS_RX 35
#define GPRS_RESET  21
#define GPRS_STATUS 34
#define GPRS_PWRKEY 22
#define GPRS_POWER_PIN  9

#define GPRS_BAUDRATE         115200


#define GPRS_PWRKEY_ON_TIME 550

#define EC25_NET_STATUS_PIN 0
#define EC25_NET_MODE_PIN 1
#define EC25_DTR_PIN 8
#define EXCECUTION_PERIOD 5000


enum class EC25_FLAGS:uint8_t {
    EC25_OK, 
    EC25_ERROR,  
    EC25_CONNECT,
    EC25_RDY,
    EC25_CGREG,
    EC25_QHTTPPOST,
    EC25_QNTP,
    EC25_QPING,
    EC25_QMTOPEN,
    EC25_QMTCONN,
    EC25_QMTSTAT,
    EC25_QMTSUB,
    EC25_CBC,
    EC25_ARROW,
    EC25_QMTPUBEX
};

enum class QUECTEL_AUTH_METHOD:uint8_t
{
    NONE=0,
    PAP,
    CHAP,
    PAP_OR_CHAP
};


class quectelEC{
    public:
        quectelEC(PCAL9535A::PCAL9535A<TwoWire> &gpio,SemaphoreHandle_t &i2cMutex);
        void            powerUp(void);
        void            powerDownGPRS(void);
        bool            isConnected(void);
        bool            pingServer(const char*, uint8_t, uint8_t);
        void            reset(void);
        uint8_t         syncNtpServer(const char *server, uint8_t port);
        void            postNtpSync(void (*)());
        uint16_t        jsonHttpPost(const char *server, const char *payload);
        void            setDeviceConfiguration(deviceConfig *configuration);
        bool            postMqttMsg(String jsonData);
        String          getIp(void);
        uint16_t        getFailedPosts(void);
        void            clearWanOperation(void);
        bool            getWanOperation(void);
        bool            getNtpStatus(void);
        uint16_t        getBattery(void);
        bool            enableMQTTReceive(void);
        bool            getInitStatus(void);
        const char *    getImei(void);

        
    private:
        void            _gprsConnectionLoop(void);
        void            _gprsSerialTask(void);
        static void     _startTaskImpl_1(void* );
        static void     _startTaskImpl_2(void* );
        void            _process_gprs(char );
        bool            _checkCreg(void);
        bool            _waitForSignal(uint16_t signalName, bool level, uint16_t waitTime);
        void            _clearFlags(void);
        uint8_t         _waitResponse(EC25_FLAGS, uint16_t);
        uint8_t         _waitResponseWithError(EC25_FLAGS, uint16_t,bool);
        bool            _checkActivatedContext(uint8_t);
        void            _getResponse(char *PTR);
        bool            _checkSecondaryContext(void);        
        bool            _configMqttServer(void);
        bool            _getImei(void);
        

        union QUECTEL_RESPONSE{
            struct{
                unsigned OK:1;   
                unsigned ERROR:1;    
                unsigned CONNECT:1;
                unsigned RDY:1;
                unsigned CGREG:1;
                unsigned QHTTPPOST:1;
                unsigned QNTP:1;
                unsigned QPING:1;
                unsigned QMTOPEN:1;
                unsigned QMTCONN:1;
                unsigned QMTSTAT:1;
                unsigned CBC:1;
                unsigned QMTSUB:1;
                unsigned ARROW:1;
                unsigned QMTPUBEX:1;
            };
            struct 
            {
                unsigned w:32;
            };
        } EC_RESPONSE;

        struct PDPCONTEXT
        {
            bool contextActivated=false;
            uint8_t protocolType;
            IPAddress ip;
        };
        struct PDPCONTEXT _deviceContexts[16];

        //private variables
        bool _wanOperation;
        bool _contextConfigured;
        bool _rebootingGprs;
        bool _gprsInitDone;
        bool _gprsConnected;
        
        bool _ntpSyncOk;
        uint16_t _battery;

        TaskHandle_t    _gprsSerialTaskHandle;
        TaskHandle_t    _gprsLoopTaskHandle;
        PCAL9535A::PCAL9535A<TwoWire> *_gpio;
        SemaphoreHandle_t *_i2cMutex;
        SemaphoreHandle_t _gprsMutex=NULL;
        uint16_t        _lastCode;

        bool            _mqttServerOpen;
        uint16_t        _failedPosts;
        deviceConfig    *_pulseConfiguration;
        char            _temptingImei[16];
        char            _Imei[16];

        void (*_postNtpSync)();
};


#endif