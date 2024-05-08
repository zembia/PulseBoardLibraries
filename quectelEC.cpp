#include "quectelEC.hpp"

const char logtag[] = "QUECTEL";


uint16_t quectelEC::getBattery(void)
{
    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(300)) == pdTRUE) 
    {
        GPRS_Serial.println("AT+CBC");
        if (_waitResponseWithError(EC25_FLAGS::EC25_CBC,300,true))
        {
            ESP_LOGD(logtag,"AT+CBC received");
        }
        xSemaphoreGive(_gprsMutex);
    }
    
    return _battery;
    

}
const char *quectelEC::getImei(void)
{
    return _Imei;
}
void quectelEC::_clearFlags(void)
{
    EC_RESPONSE.w = 0;
}
bool quectelEC::isConnected(void)
{
    return _gprsConnected;
}


bool quectelEC::_getImei(void)
{
    _clearFlags();
    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(300)) == pdTRUE) 
    {
        GPRS_Serial.println("AT+GSN");
        if (_waitResponseWithError(EC25_FLAGS::EC25_OK,300,true))
        {
            strcpy(_Imei,_temptingImei);
            ESP_LOGD(logtag,"AT+GSN received. IMEI: %s",_Imei);
        }
        else
        {
            ESP_LOGD(logtag,"AT+GSN received. IMEI not received");
        }
        xSemaphoreGive(_gprsMutex);
    }
    else
    {
        return false;
    }
    if (strlen(_Imei) == 0) return false;
    return true;
}
quectelEC::quectelEC(PCAL9535A::PCAL9535A<TwoWire> &gpio,SemaphoreHandle_t &i2cMutex)
{

    _battery = 0;
    _gpio = &gpio;
    _i2cMutex = &i2cMutex;
    _rebootingGprs = false;
    _gprsInitDone = false;
    _gprsConnected = false;
    _ntpSyncOk = false;
    _mqttServerOpen = false;
    _wanOperation   = false;
    _failedPosts    = 0;
    _ntpSyncOk      = false;

    _postNtpSync    = NULL;

    pinMode(GPRS_PWRKEY,OUTPUT);
    pinMode(GPRS_POWER_PIN,OUTPUT);
    pinMode(GPRS_RESET,OUTPUT);
    pinMode(GPRS_STATUS,INPUT);

    digitalWrite(GPRS_RESET,LOW);
    digitalWrite(GPRS_PWRKEY,LOW);
    digitalWrite(GPRS_POWER_PIN,LOW);
    GPRS_Serial.begin(GPRS_BAUDRATE,SERIAL_8N1,GPRS_RX,GPRS_TX);

    if (_gprsMutex == NULL)
    {
        _gprsMutex = xSemaphoreCreateMutex();
        if (_gprsMutex != NULL)
        {
            xSemaphoreGive(_gprsMutex);
        }
    }

    //We need to create two task. One in charge of reading
    ESP_LOGI(logtag,"configuring GPRS");
    esp_log_level_set(logtag,ESP_LOG_ERROR);
    
    xTaskCreate(
        this->_startTaskImpl_1,
        "gprsSerialTask",
        2048,
        this,
        4,
        &_gprsSerialTaskHandle
    );
    
    xTaskCreate(
        this->_startTaskImpl_2,
        "gprsLoopTask",
        4096,
        this,
        4,
        &_gprsLoopTaskHandle
    );
}
bool quectelEC::getNtpStatus(void)
{
    return _ntpSyncOk;
}
void quectelEC::_startTaskImpl_1(void *_this)
{
    ESP_LOGD(logtag,"CREARTING SERIAL TASK");
    static_cast<quectelEC*>(_this)->_gprsSerialTask();
}

void quectelEC::_startTaskImpl_2(void *_this)
{
    ESP_LOGD(logtag,"CREARTING GPRS LOOP TASK");
    static_cast<quectelEC*>(_this)->_gprsConnectionLoop();
}


void quectelEC::_gprsConnectionLoop(void)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(EXCECUTION_PERIOD);
    _contextConfigured = false;
    xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        vTaskDelay(xFrequency);
        if (_rebootingGprs) continue;
        if (_gprsInitDone == false) continue;

        //First we check our connection status
        if (!_checkCreg())
        {
            _gprsConnected = false;
            //if we are not connected, we keep waiting
            continue;
        }
        //Then, we check if we have any activated context, we use by default 1
        if (_checkActivatedContext(1) == false)
        {
            ESP_LOGI(logtag,"Contexto desactivado, activando contexto");
            if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(500)) == pdTRUE) 
            {
                _clearFlags();

                //We stop any pending request
                GPRS_Serial.println("AT+QHTTPSTOP");
                if (_waitResponseWithError(EC25_FLAGS::EC25_OK,300,true))
                {
                    ESP_LOGI(logtag,"QHTTPSTOP OK received");
                }
                else
                {
                    ESP_LOGE(logtag,"QHTTPSTOP OK not received");
                }


                //Configure context for http
                GPRS_Serial.println("AT+QHTTPCFG=\"contextid\",1");
                if (_waitResponse(EC25_FLAGS::EC25_OK,300))
                {
                    ESP_LOGI(logtag,"QHTTPCFG OK received");
                }
                else
                {
                    ESP_LOGE(logtag,"QHTTPCFG ok not received");
                    xSemaphoreGive(_gprsMutex);
                    continue;
                }
                
                //configure parameters of TCP/IP context
                //GPRS_Serial.println("AT+QICSGP=1,1,\"\",\"\" ,\"\" ,1");
                ESP_LOGD(logtag,"AT+QICSGP=1,1,\"%s\",\"%s\" ,\"%s\" ,%u\r\n",_pulseConfiguration->getAPN(),_pulseConfiguration->getApnUser(),_pulseConfiguration->getApnPassword(),_pulseConfiguration->getApnAuth());
                GPRS_Serial.printf("AT+QICSGP=1,1,\"%s\",\"%s\" ,\"%s\" ,%u\r\n",_pulseConfiguration->getAPN(),_pulseConfiguration->getApnUser(),_pulseConfiguration->getApnPassword(),_pulseConfiguration->getApnAuth());
                if (_waitResponse(EC25_FLAGS::EC25_OK,300))
                {
                    ESP_LOGI(logtag,"QICSGP OK received");
                }
                else
                {
                    ESP_LOGE(logtag,"QICSGP OK not received");
                    xSemaphoreGive(_gprsMutex);
                    continue;
                }
                ESP_LOGI(logtag,"Context activated");
                
                //We try to activate context
                GPRS_Serial.println("AT+QIACT=1");
                //Up to 150 seconds of wait
                if (_waitResponseWithError(EC25_FLAGS::EC25_OK,150000,true)) 
                {
                    ESP_LOGI(logtag,"QIACT OK received");
                }
                else
                {
                    ESP_LOGE(logtag,"QIACT OK not received");
                    xSemaphoreGive(_gprsMutex);
                    continue;
                }
                xSemaphoreGive(_gprsMutex);

                if (_checkActivatedContext(1) )
                {
                    _gprsConnected = true;
                }
            }
        }
        else
        {
            if (_gprsConnected == false)
            {
                _gprsConnected = true;
            }
        }
    }
}

void quectelEC::_gprsSerialTask(void)
{
    char incomingData;
    while(1)
    {
        if (GPRS_Serial.available())
        {
            incomingData = GPRS_Serial.read();
            //Serial.print(_incomingData);
            _process_gprs(incomingData);
        }
        else
        {
            vTaskDelay(2);
        }
    }
}


/********************FUNCION process_gprs************************/
/*  Esta funcion se llama durante una interrupcion serial, cada */
/*  vez que llega un dato por serial. Determina si se ha        */
/*  recibido un mensaje buscando por el caracter '\n'           */
/****************************************************************/
void quectelEC::_process_gprs(char input_data)
{
    static char _input_buffer[256] = {0};
    static uint16_t counter = 0;
    static uint8_t QMTSTATE = 0;
    static uint8_t qmtLenPosition=0;
    static uint16_t qmtLen = 0;
    static char *qmtData;
    static uint16_t mqttDataPointer = 0;

    //Buffer de datos de entrada
    if (counter == 255)
    {
        counter = 0;
    }
    _input_buffer[counter++]   =   input_data;


    if (input_data == '>')
    {
        EC_RESPONSE.ARROW = 1;
    }
    //Special cases
    if (counter == 10)
    {
        if (strncmp(_input_buffer,"+QMTRECV: ",10) == 0)
        {
            ESP_LOGD(logtag,"Receiving QMT DATA:");
            QMTSTATE = 1;
        }
    }

    if (counter ==15)
    {
        strncpy(_temptingImei,_input_buffer,15);
        _temptingImei[15] = 0;
    }
    switch (QMTSTATE)
    {
        //Case 0: no estamos recibiendo un mensaje QMTT
        case 0:
            //Si se recibe un avanze de linea se procesa el comando
            if (input_data=='\n')
            {        
                _input_buffer[counter]   =   0; //Para terminar la ristra.    
                ESP_LOGD(logtag,"Received serial: %s",_input_buffer);
                //Reseteamos el valor de counter, pues no se procesa la misma linea
                //dos veces
                counter = 0;
                //Parseamos el mensaje recibido    
                _getResponse(_input_buffer);                       
                _input_buffer[0]   =   0; //Para eliminar la ristra ya procesada        
            }
            break;
        case 1:
            //we wait for the first comma
            if (input_data == ',')
            {
                QMTSTATE = 2;
            }
            break;
        case 2:
            //we wait for the second comma
            if (input_data == ',')
            {
                QMTSTATE = 3;
            }
            break;
        case 3:
            //we wait for the third comma
            if (input_data == ',')
            {
                qmtLenPosition = counter;
                QMTSTATE = 4;
            }
            break;
        case 4:
            //we wait for the fourth comma
            if (input_data == ',')
            {
                qmtLen = atoi(&_input_buffer[qmtLenPosition]);
                mqttDataPointer = 0;
                ESP_LOGD(logtag,"MQTT DATA LEN: %d",qmtLen);
                qmtData = (char*)calloc(qmtLen+1,sizeof(char));
                if (qmtData == NULL)
                {
                    ESP_LOGE(logtag,"Could not allocate enough memory for mqtt data");
                }
                QMTSTATE = 5;
            }
            break;
        case 5:
            //we wait for the " character
            if (input_data == '\"')
            {
                QMTSTATE = 6;
            }
            break;
        case 6:
            if (qmtData != NULL) qmtData[mqttDataPointer++] = input_data;
            if (mqttDataPointer == qmtLen)
            {
                if (qmtData != NULL)
                {
                    qmtData[mqttDataPointer] = 0;
                    ESP_LOGD(logtag,"MQTT DATA:\r\n%s",qmtData);
                    free(qmtData);
                }
                QMTSTATE = 0;
                _input_buffer[0]   =   0; //Para eliminar la ristra ya procesada   
                counter = 0;
            }
            
            break;
    }
}

bool quectelEC::getInitStatus(void)
{
    return _gprsInitDone;
}

bool quectelEC::enableMQTTReceive(void)
{
    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(150000)) == pdTRUE) 
    {
        _clearFlags();
        GPRS_Serial.println("AT+QMTOPEN?");
        if (_waitResponse(EC25_FLAGS::EC25_OK,300))
        {
            ESP_LOGD(logtag,"AT+QMTOPEN OK received");
        }
        xSemaphoreGive(_gprsMutex);
    }
    else
    {
        ESP_LOGE(logtag,"COULD NOT GET SEMAPHORE");
        return false;
    }
    if (_waitResponse(EC25_FLAGS::EC25_QMTOPEN,0))
    {
        ESP_LOGI(logtag,"QMT already been opened");
    }
    else
    {
        ESP_LOGI(logtag,"QMT not opened yet");
        if (!_configMqttServer())
        {
            return false;
        }
    }


    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(150000)) == pdTRUE) 
    {
        _clearFlags();
        GPRS_Serial.printf("AT+QMTSUB=0,1,\"%s\",1\r\n",_pulseConfiguration->getTopic());
        ESP_LOGI(logtag,"AT+QMTSUB=0,1,\"%s\",1\r\n",_pulseConfiguration->getTopic());
        if (_waitResponse(EC25_FLAGS::EC25_OK,300))
        {
            ESP_LOGD(logtag,"AT+QMTSUB received");
        }
        xSemaphoreGive(_gprsMutex);
    }
    else
    {
        ESP_LOGE(logtag,"COULD NOT GET SEMAPHORE");
        return false;
    }
}

bool quectelEC::getWanOperation(void)
{
    return _wanOperation;
}

void quectelEC::clearWanOperation(void)
{
    _wanOperation = false;
}

void quectelEC::powerUp(void)
{
    _clearFlags();
    ESP_LOGD(logtag,"Powering up GPRS");
    if (_i2cMutex == NULL)
    {
        ESP_LOGE(logtag, "ERROR, i2cMutex is not initialized");
    }
    if (xSemaphoreTake(*_i2cMutex, (TickType_t ) 100) == pdTRUE) 
    {
        ESP_LOGD(logtag,"Setting EC25_NET_STATUS_PIN PinMode");
        _gpio->pinMode(EC25_NET_STATUS_PIN,1);
        ESP_LOGD(logtag,"Setting EC25_NET_MODE_PIN PinMode");
        _gpio->pinMode(EC25_NET_MODE_PIN,1);
        ESP_LOGD(logtag,"Driving EC25_NET_MODE_PIN LOW");
        _gpio->digitalWrite(EC25_NET_MODE_PIN,LOW);
        ESP_LOGD(logtag,"Driving EC25_NET_STATUS_PIN LOW");
        _gpio->digitalWrite(EC25_NET_STATUS_PIN,LOW);

        ESP_LOGD(logtag,"Setting EC25_DRT_PIN PinMode");
        _gpio->pinMode(EC25_DTR_PIN,1);
        ESP_LOGD(logtag,"Driving EC25_DTR_PIN LOW");
        _gpio->digitalWrite(EC25_DTR_PIN,LOW);
        xSemaphoreGive(*_i2cMutex);
    }
    else
    {
        ESP_LOGE(logtag,"ERROR gettig I2c mutex");
        return;
    }
    ESP_LOGI(logtag,"Powering up GPRS PSU");
    Serial.flush();
    _clearFlags();
    digitalWrite(GPRS_POWER_PIN,HIGH);
    vTaskDelay(pdMS_TO_TICKS(30));
    for (int i=0;i<5;i++)
    {
        ESP_LOGD(logtag,"Driving PWRKEY DOWN");
        
        digitalWrite(GPRS_PWRKEY,HIGH);
        delay(GPRS_PWRKEY_ON_TIME);
        digitalWrite(GPRS_PWRKEY,LOW);

        
        ESP_LOGD(logtag,"PWRKEY released");
        
        if (_waitForSignal(GPRS_STATUS,0,3000))
        {
            ESP_LOGD(logtag,"GPRS Powered on");
            
            if (xSemaphoreTake(*_i2cMutex, (TickType_t )100) == pdTRUE) 
            {
                _gpio->pinMode(EC25_NET_MODE_PIN,0);
                _gpio->pinMode(EC25_NET_STATUS_PIN,0);
                _gpio->pinMode(EC25_DTR_PIN,0);
                xSemaphoreGive(*_i2cMutex);
            }
            break;
        }
        /*if (EC_RESPONSE.RDY)
        {
            break;
        }*/
    }
    //We receive a RDY text when the modem is ready for AT commands

    if (!_waitResponse(EC25_FLAGS::EC25_RDY,9000))
    {
        ESP_LOGD(logtag,"RDY not received");
    }
    else
    {
        ESP_LOGD(logtag,"RDY received");
    }
    
    for (int i=0;i<10;i++)
    {
        if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(500)) == pdTRUE) 
        {
            ESP_LOGD(logtag,"Sent: AT");
            GPRS_Serial.println("AT");
            xSemaphoreGive(_gprsMutex);
        }
        if (_waitResponse(EC25_FLAGS::EC25_OK,500))
        {
            ESP_LOGD(logtag,"AT OK received");
            break;
        }
        
    }
    //Only for quectel EC200?
    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(500)) == pdTRUE) 
    {
        ESP_LOGD(logtag,"Sent: AT+QSCLKEX=0");
        GPRS_Serial.println("AT+QSCLKEX=0");
        xSemaphoreGive(_gprsMutex);
    }
    if (_waitResponse(EC25_FLAGS::EC25_OK,300))
    {
        ESP_LOGD(logtag,"AT+QSCLKEX OK received");
    }

    for (int i=0;i<5;i++)
    {
        if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(500)) == pdTRUE) 
        {
            ESP_LOGD(logtag,"Sent: ATE0");
            GPRS_Serial.println("ATE0");
            xSemaphoreGive(_gprsMutex);
            
        }
        if (_waitResponse(EC25_FLAGS::EC25_OK,300))
        {
            ESP_LOGD(logtag,"ATE0 OK received");
            break;
        }
        vTaskDelay(100);
    }

    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(500)) == pdTRUE) 
    {
        ESP_LOGD(logtag,"Sent: AT+CMEE=2");
        GPRS_Serial.println("AT+CMEE=2");
        xSemaphoreGive(_gprsMutex);
    }
    if (_waitResponse(EC25_FLAGS::EC25_OK,300))
    {
        ESP_LOGD(logtag,"AT+CMEE=2 OK received");
    }

    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(500)) == pdTRUE) 
    {
        ESP_LOGD(logtag,"Sent: AT+QURCCFG=\"urcport\",\"uart1\"");
        GPRS_Serial.println("AT+QURCCFG=\"urcport\",\"uart1\"");
        xSemaphoreGive(_gprsMutex);
    }
    if (_waitResponse(EC25_FLAGS::EC25_OK,300))
    {
        ESP_LOGD(logtag,"AT+QURCCFG=\"urcport\",\"uart1\" OK received");
    }




    ESP_LOGD(logtag,"Getting IMEI");

    for (int i=0 ;i<20;i++)
    {
        if (_getImei())
        {
            break;
        }
        else
        {
            vTaskDelay(400);
        }
    }

    //GPRS_Serial.println("AT+IPR=115200;&W");
    _gprsInitDone = true;



}


bool quectelEC::_waitForSignal(uint16_t signalName,bool level,uint16_t waitTime)
{
    unsigned long int time;
    bool receivedSignal = false;
    time = millis();
    do
    {
        if (digitalRead(signalName) == level)
        {
            receivedSignal = true;
            break;
        }
        vTaskDelay(2); //we delay for 2 ticks
    } while (millis()-time<waitTime);
    return receivedSignal;    
    
}

void quectelEC::powerDownGPRS(void)
{
    ESP_LOGI(logtag,"powering down modem");
    GPRS_Serial.write("AT+QPOWD\r\n");
    _waitForSignal(GPRS_STATUS,1,8000);
    digitalWrite(GPRS_POWER_PIN,LOW);
    ESP_LOGI(logtag,"Modem OFF");
}



bool quectelEC::_checkCreg(void)
{
    static bool lastConnStatus = false;
    ESP_LOGD(logtag,"checking CREG");
    if (_rebootingGprs) return 0;
    _clearFlags();

    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(10000)) == pdTRUE) 
    {
        GPRS_Serial.println("AT+CGREG?");
        if (_waitResponse(EC25_FLAGS::EC25_CGREG,300))
        {
            ESP_LOGI(logtag,"AT+CGREG? OK received");
        }
        xSemaphoreGive(_gprsMutex);
    }
    else
    {
        return lastConnStatus;
    }

    _clearFlags();

    if ((_lastCode==5) || (_lastCode == 1))
    {
        ESP_LOGI(logtag,"REGISTERED IN NETWORK. CREG %d",_lastCode);
        lastConnStatus = true;
        
    }
    else
    {
        if (_lastCode == 3)
        {
            ESP_LOGE(logtag,"NETWORK REGISTRATION DENIED");
        }
        else if (_lastCode == 0)
        {
            if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(10000)) == pdTRUE) 
            {
                ESP_LOGD(logtag,"AT+CGATT=1");
                GPRS_Serial.println("AT+CGATT=1");
                if (_waitResponse(EC25_FLAGS::EC25_OK,500))
                {
                    ESP_LOGI(logtag,"AT+CGATT=1 OK received");
                }
                xSemaphoreGive(_gprsMutex);
            }
            else
            {
                return lastConnStatus;
            }
        }
        else
        {
            ESP_LOGE(logtag, "Registrando. CREG: %d",_lastCode);
        }
        lastConnStatus = false;
    }
    return lastConnStatus;
}

/********************FUNCION wait_response***********************/
/*   Esta funcion retorna un 1, si es que se ha recibido el     */
/*  mensjae esperado en una interrupcion. Si no se ha recibido  */
/*  el mensaje despues de el timeout especificado, se retorna   */
/*  un 0.                                                       */
/****************************************************************/
uint8_t quectelEC::_waitResponse(EC25_FLAGS RESPONSE,uint16_t milliseconds)
{
    return _waitResponseWithError(RESPONSE,milliseconds,false);
}


/********************FUNCION wait_response WITH ERROR*************/
/*   Esta funcion retorna un 1, si es que se ha recibido el     */
/*  mensjae esperado en una interrupcion. Si no se ha recibido  */
/*  el mensaje despues de el timeout especificado, se retorna   */
/*  un 0. En caso de recibir un error, retorna de inmediato     */
/****************************************************************/

uint8_t quectelEC::_waitResponseWithError(EC25_FLAGS RESPONSE,uint16_t milliseconds, bool exitWithError)
{
    if (milliseconds > 0)
    {
        ESP_LOGD(logtag,"Waiting for reply");
    }
    static unsigned long StartTime;
    StartTime = millis();
    do{
        if (exitWithError)
        {
            if (EC_RESPONSE.ERROR)
            {
                EC_RESPONSE.ERROR = 0;
                return 0;
            }
        }
        switch (RESPONSE)
        {
            case EC25_FLAGS::EC25_OK:
                if (EC_RESPONSE.OK)
                {
                    EC_RESPONSE.OK = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_ERROR:
                if (EC_RESPONSE.ERROR)
                {
                    EC_RESPONSE.ERROR = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_CONNECT:
                if (EC_RESPONSE.CONNECT)
                {
                    EC_RESPONSE.CONNECT = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_RDY:
                if (EC_RESPONSE.RDY)
                {
                    EC_RESPONSE.RDY = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_CGREG:
                if (EC_RESPONSE.CGREG)    
                {
                    EC_RESPONSE.CGREG = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_QHTTPPOST:
                if (EC_RESPONSE.QHTTPPOST)
                {
                    EC_RESPONSE.QHTTPPOST = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_QNTP:
                if (EC_RESPONSE.QNTP)
                {
                    EC_RESPONSE.QNTP = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_QPING:
                if (EC_RESPONSE.QPING)
                {
                    EC_RESPONSE.QPING = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_QMTOPEN:
                if (EC_RESPONSE.QMTOPEN)
                {
                    EC_RESPONSE.QMTOPEN = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_QMTCONN:
                if (EC_RESPONSE.QMTCONN)
                {
                    EC_RESPONSE.QMTCONN = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_QMTSTAT:
                if (EC_RESPONSE.QMTSTAT)
                {   
                    EC_RESPONSE.QMTSTAT = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_CBC:
                if (EC_RESPONSE.CBC)
                {
                    EC_RESPONSE.CBC = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_QMTSUB:
                if (EC_RESPONSE.QMTSUB)
                {
                    EC_RESPONSE.QMTSUB = 0;
                    return 1;
                }
                break;
            case EC25_FLAGS::EC25_ARROW:
                if (EC_RESPONSE.ARROW)
                {
                    EC_RESPONSE.ARROW = 0;
                    return 1;
                }
            case EC25_FLAGS::EC25_QMTPUBEX:
                if (EC_RESPONSE.QMTPUBEX)
                {
                    EC_RESPONSE.QMTPUBEX = 0;
                    return 1;
                }
            default:
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
        //Serial.print(".")
    }while( (millis() - StartTime) < milliseconds );
    ESP_LOGD(logtag,"Reply Timed out");
    return 0;
}


/********************FUNCION get_response************************/
/*  Esta funcion revisa si se recibio una ristra en particular  */
/*  en el mensaje recibido desde el modulo GPRS, dependiendo de */
/*  la ristra recibida se suben flags indicando que se recibio  */
/*  determinado mensaje. Esta funcion se llama en process_gprs  */
/*  uncamente, la cual a su vez es llamada en una interrupcion  */
/****************************************************************/

void quectelEC::_getResponse(char *PTR)
{
    char *token = NULL;
    if (strncmp(PTR,"OK",2)== 0)
    {
        EC_RESPONSE.OK   =   1;
        return;
    }
    if (strncmp(PTR,"ERROR",5)==0)
    {
        EC_RESPONSE.ERROR   =   1;
        return;

    }
    if (strncmp(PTR,"+CME ERROR",10)==0)
    {
        EC_RESPONSE.ERROR   =   1;
        return;
    }
    if (strncmp(PTR,"CONNECT",7)==0)
    {
        EC_RESPONSE.CONNECT   =   1;
        return;
    }
    if (strncmp(PTR,"RDY",3)==0)
    {
        EC_RESPONSE.RDY   =   1;
        return;
    }
    if (strncmp(PTR,"+CGREG: 0,",10) == 0)
    {
        EC_RESPONSE.CGREG = 1;
        _lastCode = atoi(&PTR[10]);
        return;
    }

    if (strncmp(PTR,"+QHTTPPOST: 0,",14) == 0)
    {
        EC_RESPONSE.QHTTPPOST = 1;
        _lastCode = atoi(&PTR[14]);
        return;
    }

    if (strncmp(PTR,"+QMTOPEN: 0,",12) == 0)
    {
        EC_RESPONSE.QMTOPEN = 1;
        _lastCode = atoi(&PTR[12]);
        return;
    }

    if (strncmp(PTR,"+QPING: 0,",10) == 0)
    {
        EC_RESPONSE.QPING = 1; 
        return;
    }

    if (strncmp(PTR,"+QMTCONN: 0,0,0,",15) == 0)        
    {
        EC_RESPONSE.QMTCONN = 1;
        return;
    }
    if (strncmp(PTR,"+QMTSTAT: 0,",12) == 0)
    {
        _lastCode = atoi(&PTR[12]);
        EC_RESPONSE.QMTSTAT = 1;
        if (_lastCode == 1 || _lastCode == 5 || _lastCode == 6 || _lastCode == 7)
        {
            _mqttServerOpen = false;
        }
        return;

    }
    if (strncmp(PTR,"+QIACT:",7 ) == 0)
    {
        //There is an active context, we need to update the index
        //First we get the context index
        uint8_t PDPContextId;
        token = strtok(&PTR[7],":,\""); if (token == NULL) return;
        PDPContextId = atoi(token) - 1;
        if (PDPContextId >15) return; //validate data
        
        //Then we get wether it is active or not
        token = strtok(NULL,":,\""); if (token == NULL) return;
        _deviceContexts[PDPContextId].contextActivated = (atoi(token) == 0)? false:true;

        //then we get the protocol type
        token = strtok(NULL,":,\""); if (token == NULL) return;
        _deviceContexts[PDPContextId].protocolType = atoi(token);

        //then we get the first byte of the IP
        token = strtok(NULL,":,\""); if (token == NULL) return;
        _deviceContexts[PDPContextId].ip.fromString(token);
    }
    if (strncmp(PTR,"+CBC: ",6) == 0 )
    {
        token = strtok(&PTR[6],","); if (token == NULL) return;
        token = strtok(NULL,","); if (token == NULL) return;
        token = strtok(NULL,","); if (token == NULL) return;
        _battery = atoi(token);
        EC_RESPONSE.CBC = 1;
    }

    if (strncmp(PTR,"+QMTPUBEX: 0,1,0",16) == 0)
    {
        EC_RESPONSE.QMTPUBEX = 1;
        return;
    }

    if (strncmp(PTR,"+QNTP: ",7)==0)
    {
        _ntpSyncOk = false;
        //first we get the error code
        token = strtok(&PTR[7],":,\"+/"); if (token == NULL) return;
        _lastCode = atoi(token);

        EC_RESPONSE.QNTP = 1;
        if (_lastCode != 0) return;
        
        uint16_t tempValue;
        struct tm t_tm;
        struct timeval val;

        //We get the year first
        token = strtok(NULL,":,\"+/"); if (token == NULL) return;
        tempValue = atoi(token);
        t_tm.tm_year = tempValue - 1900;    //Year, whose value starts from 1900

        //Then we get the month
        token = strtok(NULL,":,\"+/"); if (token == NULL) return;
        tempValue = atoi(token);
        t_tm.tm_mon = tempValue - 1;       //Month (starting from January, 0 for January) - Value range is [0,11]
        
        //Then the day
        token = strtok(NULL,":,\"+/"); if (token == NULL) return;
        tempValue = atoi(token);
        t_tm.tm_mday = tempValue;

        //then the hour
        token = strtok(NULL,":,\"+/"); if (token == NULL) return;
        tempValue = atoi(token);
        t_tm.tm_hour = tempValue;

        //then the minute
        token = strtok(NULL,":,\"+/"); if (token == NULL) return;
        tempValue = atoi(token);
        t_tm.tm_min = tempValue;

        //then the seconds
        //then the minute
        token = strtok(NULL,":,\"+/"); if (token == NULL) return;
        tempValue = atoi(token);
        t_tm.tm_sec = tempValue;
        
        
        
        val.tv_sec = mktime(&t_tm);
        val.tv_usec = 0;
        
        //val.tv_sec-=4*3600; //
        settimeofday(&val, NULL);
        _ntpSyncOk = true;
        
    }
}

//#define PRINTACTIVATEDCONTEXTS
bool quectelEC::_checkActivatedContext(uint8_t index)
{
    ESP_LOGD(logtag,"CHECKING ACTIVATED CONTEXT");
    if (_rebootingGprs) return false;

    if (index>16 || index == 0)
    {
        return false;
    }
    _clearFlags();
    uint8_t i;
    for (i= 0;i<16;i++)
    {
        _deviceContexts[i].contextActivated=false;
    }

    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(10000)) == pdTRUE) 
    {
        GPRS_Serial.println("AT+QIACT?");
        if (_waitResponse(EC25_FLAGS::EC25_OK,300))
        {
            ESP_LOGI(logtag,"AT+QIACT? OK received");
        }
        xSemaphoreGive(_gprsMutex);
    }
    else
    {
        return false;
    }

    #ifdef PRINTACTIVATEDCONTEXTS
        
        for (i=0;i<16;i++)
        {
            if (_deviceContexts[i].contextActivated)
            {
                Serial.print("Context ");Serial.print(i+1);Serial.print(" ");
                Serial.print(_deviceContexts[i].contextActivated?"Activated":"deactivated");
                Serial.print(" IP: ");Serial.println(_deviceContexts[i].ip.toString());
            }
        }
    #endif


    if (_deviceContexts[index-1].contextActivated)
    {
        return true;
    }
    else
    {
        return false;
    }
}


bool quectelEC::_checkSecondaryContext(void)
{
    if (_rebootingGprs) return 0;
    if (_checkActivatedContext(2) == false)
    {
        ESP_LOGI(logtag,"Contexto secundario desactivado, activando contexto");    
        if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(500)) == pdTRUE) 
        {
            _clearFlags();
            //configure parameters of TCP/IP context
            //GPRS_Serial.println("AT+QICSGP=2,1,\"\",\"\" ,\"\" ,1");
            //GPRS_Serial.println("AT+QICSGP=2,1,\"data.mono\",\"\" ,\"\" ,1");
            GPRS_Serial.printf("AT+QICSGP=2,1,\"%s\",\"%s\" ,\"%s\" ,%u\r\n",_pulseConfiguration->getAPN(),_pulseConfiguration->getApnUser(),_pulseConfiguration->getApnPassword(),_pulseConfiguration->getApnAuth());
            if (_waitResponse(EC25_FLAGS::EC25_OK,300))
            {
                ESP_LOGI(logtag,"QICSGP OK received");
            }
            else
            {
                ESP_LOGE(logtag,"QICSGP OK not received");
                xSemaphoreGive(_gprsMutex);
                return 0;
            }
            ESP_LOGI(logtag,"NTP Context activated");
            //We try to activate context
            GPRS_Serial.println("AT+QIACT=2");
            //Up to 150 seconds of wait
            if (_waitResponseWithError(EC25_FLAGS::EC25_OK,150000,true)) 
            {
                ESP_LOGI(logtag,"QIACT OK received");
            }
            else
            {
                ESP_LOGE(logtag,"QIACT OK not received");
                xSemaphoreGive(_gprsMutex);
                return 0;
            }
            xSemaphoreGive(_gprsMutex);
        }
    }
    return 1;
}


bool quectelEC::pingServer(const char* server,uint8_t timeout,uint8_t nPings)
{   
    if (_rebootingGprs) return 0;

    bool pingSuccess = true;

    if ((server == NULL) || (timeout == 0) || (nPings==0) || (nPings>10))
    {
        return 0;
    }
    if (_checkSecondaryContext() == 0)
    {
        return 0;
    }
    //If we reached this point, the context is active
    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(500)) == pdTRUE) 
    {
        GPRS_Serial.printf("AT+QPING=2,\"%s\",%d\r\n",server,timeout);


        if (_waitResponseWithError(EC25_FLAGS::EC25_OK,500,true)) 
        {
            ESP_LOGI(logtag,"AT+QPING OK received");
        }
        else
        {
            ESP_LOGE(logtag,"AT+QPING OK not received");
            xSemaphoreGive(_gprsMutex);
            return 0;
        }

        _clearFlags();
        for (uint8_t i=0;i<nPings+1;i++)
        {
            if (_waitResponse(EC25_FLAGS::EC25_QPING,1000*timeout+300) == 0)
            {
                pingSuccess = false;
                break;
            }
        }
        
        xSemaphoreGive(_gprsMutex);
    }
    if (pingSuccess==true) _wanOperation = true;
    return pingSuccess;

}



void quectelEC::reset(void)
{
    _gprsConnected = false;
    _rebootingGprs = true;
    _failedPosts = 0;

    
    if (xSemaphoreTake(*_i2cMutex, (TickType_t ) 100) == pdTRUE) 
    {
        _gpio->pinMode(EC25_NET_MODE_PIN,1);
        _gpio->digitalWrite(EC25_NET_MODE_PIN,LOW);
        _gpio->pinMode(EC25_NET_STATUS_PIN,1);
        _gpio->digitalWrite(EC25_NET_STATUS_PIN,LOW);
        _gpio->pinMode(EC25_DTR_PIN,1);
        _gpio->digitalWrite(EC25_DTR_PIN,LOW);
        xSemaphoreGive(*_i2cMutex);
    }
    else
    {
        ESP_LOGE(logtag,"ERROR gettig I2c mutex");
        return;
    }


    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(150000)) == pdTRUE) 
    {
        powerDownGPRS();
        xSemaphoreGive(_gprsMutex);
    }
    else
    {
        ESP_LOGE(logtag,"Could not take semaphore");
        _rebootingGprs = false;
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    
    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(10000)) == pdTRUE) 
    {

        powerUp();
        xSemaphoreGive(_gprsMutex);
    }
    else
    {
        ESP_LOGE(logtag,"Could not take semaphore");
        _rebootingGprs = false;
        return;
    }

    _rebootingGprs = false;
    
}

uint16_t quectelEC::jsonHttpPost(const char *server, const char *payload)
{
    uint16_t httpReturnCode = 0;
    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(5000)) == pdTRUE) 
    {
        _clearFlags();
        //HTTP CONNECT
        //Configuring application/json        
        
        GPRS_Serial.println("AT+QHTTPSTOP");
        if (_waitResponseWithError(EC25_FLAGS::EC25_OK,300,true))
        {
            ESP_LOGI(logtag,"QHTTPSTOP OK received");
        }
        else
        {
             ESP_LOGE(logtag,"QHTTPSTOP OK not received");
        }

        GPRS_Serial.println("AT+QHTTPCFG=\"contenttype\",4");
        if (_waitResponse(EC25_FLAGS::EC25_OK,300))
        {
            ESP_LOGI(logtag,"QHTTPCFG OK received");
        }
        else
        {
           
            ESP_LOGE(logtag,"QHTTPCFG OK not received");
            xSemaphoreGive(_gprsMutex);
            _failedPosts++;
            return 0;
        }
        
        GPRS_Serial.print("AT+QHTTPURL=");
        GPRS_Serial.print(strlen(server));
        GPRS_Serial.println(",1");

        if (_waitResponse(EC25_FLAGS::EC25_CONNECT,1000))
        {
            ESP_LOGI(logtag,"QHTTPURL CONNECT Received");
        }
        else
        {
            ESP_LOGE(logtag,"QHTTPURL CONNECT not received. Deactivating context");            
        }
        GPRS_Serial.println(server);
        
        if (_waitResponse(EC25_FLAGS::EC25_OK,5000))
        {
            ESP_LOGI(logtag,"QHTTPURL success");
        }
        else
        {
            ESP_LOGE(logtag,"QHTTPURL failed");
            xSemaphoreGive(_gprsMutex);
            _failedPosts++;
            return 0;
        }
        xSemaphoreGive(_gprsMutex);
    }
    else
    {
        ESP_LOGE(logtag,"gprs serial busy");
        _failedPosts++;
        return 0;
    }
    //HTTP POST

    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(5000)) == pdTRUE) 
    {
        _clearFlags();
        GPRS_Serial.print("AT+QHTTPPOST=");
        GPRS_Serial.print(strlen(payload));
        GPRS_Serial.println(",1,60");

        if (_waitResponseWithError(EC25_FLAGS::EC25_CONNECT,80000,true))
        {
            ESP_LOGI(logtag,"QHTTPPOST CONNECT Received");
        }
        else
        {
            ESP_LOGE(logtag,"QHTTPPOST CONNECT not received");
            xSemaphoreGive(_gprsMutex);
            _failedPosts++;
            return 0;
        }
        _clearFlags();
        GPRS_Serial.print(payload);
        ESP_LOGD(logtag,"postData: %s",payload);

        if (_waitResponse(EC25_FLAGS::EC25_OK,500))
        {
            ESP_LOGI(logtag,"OK Received");
        }
        else
        {
            ESP_LOGE(logtag,"OK not received");
            xSemaphoreGive(_gprsMutex);
            _failedPosts++;
            return 0;
        }

        if (_waitResponseWithError(EC25_FLAGS::EC25_QHTTPPOST,60000,true))
        {
            ESP_LOGI(logtag,"EC25_QHTTPPOST Received: %d",_lastCode);
            httpReturnCode = _lastCode;
        }
        else
        {
            ESP_LOGE(logtag,"EC25_QHTTPPOST not received");
            xSemaphoreGive(_gprsMutex);
            _failedPosts++;
            return 0;
        }
        xSemaphoreGive(_gprsMutex);
    }
    else
    {
        ESP_LOGE(logtag,"gprs serial busy");
        _failedPosts++;
        return 0;
    }

    _wanOperation = true;
    _failedPosts = 0;
    return httpReturnCode;
}


uint8_t quectelEC::syncNtpServer(const char *server, uint8_t port)
{
    if (_rebootingGprs) return 255;
    if (_checkSecondaryContext() == 0)
    {
        return 255;
    }
    //If we reached this point, the context is active
    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(1000)) == pdTRUE) 
    {
        GPRS_Serial.print("AT+QNTP=2,\"");
        GPRS_Serial.print(server);
        GPRS_Serial.println("\",123,0");

        if (_waitResponseWithError(EC25_FLAGS::EC25_QNTP,150000,true)) 
        {
            ESP_LOGI(logtag,"QNTP OK received");
        }
        else
        {
            ESP_LOGE(logtag,"QNTP OK not received");
            xSemaphoreGive(_gprsMutex);
            return 0;
        }
        xSemaphoreGive(_gprsMutex);
    }
    else
    {
        return 0; //Could not get handle of the serial port
    }

    if (_ntpSyncOk)
    {
        struct tm timeinfo;
        char localtimeBuffer[30];
        if (!getLocalTime(&timeinfo)) {
          Serial.println("Failed to obtain time in getFormattedDateTime");
          return 0;
      }

      if (_postNtpSync)
      {
        _postNtpSync();
      }
      else
      {
        ESP_LOGW(logtag,"WARNING: NTP SYNC FUNCTION NOT SET");
      }
      strftime(localtimeBuffer, 30, "%FT%T%Z", &timeinfo);
      ESP_LOGI(logtag,"NTP TIME SYNCED CORRECTLY: %s",localtimeBuffer);
      

    }
    else
    {
        ESP_LOGE(logtag,"NTP TIME NOT SYNCED");
        return 0;
    }
    _wanOperation = true;
    return 1;
}


void quectelEC::postNtpSync(void (*callbackFunction)())
{
    _postNtpSync = callbackFunction;
    ESP_LOGD(logtag,"NTP Callback function set");
}



bool quectelEC::_configMqttServer(void)
{
    ESP_LOGI(logtag,"configuring mqttserver");
    if (_pulseConfiguration == NULL)
    {
        return false;
    }

    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(10000)) == pdTRUE) 
    {
        //First we check if the QMT is open
         _clearFlags();  
        ESP_LOGD(logtag,"AT+QMTOPEN?");
        GPRS_Serial.println("AT+QMTOPEN?");
        if (_waitResponseWithError(EC25_FLAGS::EC25_OK,1000,true))
        {
            ESP_LOGI(logtag,"AT+QMTOPEN? OK received");
        }
        if (EC_RESPONSE.QMTOPEN)
        {
            ESP_LOGD(logtag,"AT+QMTCLOSE=0");
            GPRS_Serial.println("AT+QMTCLOSE=0");

            if (_waitResponseWithError(EC25_FLAGS::EC25_OK,30000,true))
            {
                ESP_LOGI(logtag,"AT+QMTCLOSE OK received");
            }
            xSemaphoreGive(_gprsMutex);
            return false;
        }

        _clearFlags();       
        ESP_LOGD(logtag,"AT+QMTCFG=\"recv/mode\",0,0,1");
        GPRS_Serial.println("AT+QMTCFG=\"recv/mode\",0,0,1");
        if (_waitResponse(EC25_FLAGS::EC25_OK,300))
        {
            ESP_LOGI(logtag,"AT+QMTCFG OK received");
        }
        else
        {
            xSemaphoreGive(_gprsMutex);
            return false;
        }

        _clearFlags();
        
        ESP_LOGD(logtag,"AT+QMTOPEN=0,\"%s\",%d\r\n",_pulseConfiguration->getUrl(),_pulseConfiguration->getPort());
        GPRS_Serial.printf("AT+QMTOPEN=0,\"%s\",%d\r\n",_pulseConfiguration->getUrl(),_pulseConfiguration->getPort());
        if (_waitResponseWithError(EC25_FLAGS::EC25_QMTOPEN,120000,true))
        {
            ESP_LOGI(logtag,"AT+QMTOPEN received");
        }
        else
        {
            xSemaphoreGive(_gprsMutex);
            return false;
        }

        if (_lastCode != 0)
        {
            ESP_LOGE(logtag,"Error opening mqqt %d",_lastCode);
            xSemaphoreGive(_gprsMutex);
            return false;    
        }

        _clearFlags();
        /*ESP_LOGD(logtag,"AT+QMTCONN=0,\"%s\",\"%s\",\"%s\"\r\n",_Imei,_pulseConfiguration->getUser(),_pulseConfiguration->getPassword());
        GPRS_Serial.printf("AT+QMTCONN=0,\"%s\",\"%s\",\"%s\"\r\n",_Imei,_pulseConfiguration->getUser(),_pulseConfiguration->getPassword());
        */
        ESP_LOGD(logtag,"AT+QMTCONN=0,\"%s\",\"%s\",\"%s\"\r\n","TESTZEMBIA",_pulseConfiguration->getUser(),_pulseConfiguration->getPassword());
        GPRS_Serial.printf("AT+QMTCONN=0,\"%s\",\"%s\",\"%s\"\r\n","TESTZEMBIA",_pulseConfiguration->getUser(),_pulseConfiguration->getPassword());
        if (_waitResponseWithError(EC25_FLAGS::EC25_QMTCONN,5000,true))
        {
            ESP_LOGI(logtag,"+QMTCONN received");
        }
        else
        {
            ESP_LOGE(logtag,"+QMTCONN not received");
            xSemaphoreGive(_gprsMutex);
            return false;
        }

        xSemaphoreGive(_gprsMutex);
    }
    else
    {
        return false;
    }
    _mqttServerOpen = true;
    return true;
}

void quectelEC::setDeviceConfiguration(deviceConfig *configuration)
{
    ESP_LOGD(logtag,"deviceConfiguration set");
    _pulseConfiguration = configuration;
}



bool quectelEC::postMqttMsg(String jsonData)
{
    if (_pulseConfiguration == NULL)
    {
        ESP_LOGE(logtag,"pulseConfiguration NOT SET");
        return false;
    }
    if (_rebootingGprs) return false;
    if (_mqttServerOpen == false )
    {
        if (!_configMqttServer())
        {
            return false;
        }
    }
    if (xSemaphoreTake(_gprsMutex, (TickType_t ) pdMS_TO_TICKS(10000)) == pdTRUE) 
    {
        _clearFlags();       
        ESP_LOGD(logtag,"AT+QMTPUBEX=0,1,2,0,\"%s\",%u\r\n",_pulseConfiguration->getTopic(),jsonData.length());
        GPRS_Serial.printf("AT+QMTPUBEX=0,1,2,0,\"%s\",%u\r\n",_pulseConfiguration->getTopic(),jsonData.length());
        //we need to wait for a single >
        _clearFlags();
        if (_waitResponseWithError(EC25_FLAGS::EC25_ARROW,15000,true))
        {
            ESP_LOGI(logtag,"> received");
        }

        
        ESP_LOGD(logtag,"%s",jsonData.c_str());
        GPRS_Serial.print(jsonData.c_str());
        if (_waitResponseWithError(EC25_FLAGS::EC25_QMTPUBEX,15000,true))
        {
            ESP_LOGI(logtag,"AT+QMTPUBEX OK received");
        }
        else
        {
            _clearFlags();
            ESP_LOGD(logtag,"AT+QMTOPEN?");
            GPRS_Serial.println("AT+QMTOPEN?");
            if (_waitResponseWithError(EC25_FLAGS::EC25_OK,1000,true))
            {
                ESP_LOGI(logtag,"AT+QMTOPEN? OK received");
            }
            if (EC_RESPONSE.QMTOPEN == 0)
            {
                _mqttServerOpen = false;

            }            
            xSemaphoreGive(_gprsMutex);
            return false;
        }
        xSemaphoreGive(_gprsMutex);
    }
     _wanOperation = true;
    return true;
}

String quectelEC::getIp(void)
{
    return _deviceContexts[0].ip.toString();
}

uint16_t quectelEC::getFailedPosts(void)
{
    return _failedPosts;
}