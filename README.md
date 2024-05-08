# PulseBoardLibraries

Estas bibliotecas están pensadas para ser usadas con placa pulse, es por ello que se asumen algunas cosas que pueden generar problemas en otras PCB.

## deviceConfig.hpp
Esta archivo contiene 3 clases distintas:
1. deviceConfig: guarda diversas configuraciones de la placa como credenciales, dirección y tópico mqtt, url http, apn, etc
2. pulseIoPort: Genera un puerto digital usando un I/O expander
3. pulseAiPort: Genera un puerto analogo

### deviceConfig
Esta clase es principalmente para configuración del dispositivo. Debido a que usa la librería preference y por limitaciones de Arduino, esta no puede ser declarada como una variable global. Sin embargo, se puede declarar un puntero a la configuración e inicializarlo en el setup. Esto es debido a que Arduino no incializa la memoria no volatil hasta entrar en Setup. Ejemplo:
``` c++
deviceConfig *pulseConfiguration;

void setup()
{
  pulseConfiguration = new deviceConfig();
}
```
Esto inicializara la memoria no volatil y cargara en la clase todas las configuraciones previamente realizadas
La clase deviceConfig tiene diversas funciones para distintas aplicaciones y la mayoría de ellas tiene valores por defecto, por lo que no es necesario llamar a todas los metodos para configurar el equipo

#### configAPN
Este método almacena en memoria no volatil la configuración del APN, se debe llamar a la función configAPN de la siguiente manera:

``` c++
deviceConfig *pulseConfiguration;
#define APN "bam.entelpcs.cl"
#define APN_PASS ""
#define APN_AUTH 1
void setup()
{
  pulseConfiguration = new deviceConfig();
  pulseConfiguration->configAPN(APN,APN_PASS,APN_AUTH)
}
```
usuario y contraseña pueden ir vacios, APN es obligatorio. La autenticacion es como sigue:

0 - NONE  
1 - PAP  
2 - CHAP  
3 - PAP or CHAP  

Valores por defecto de apn, contraseña y autenticación son "data.mono", "", y 1.

#### configHttp

#### configMqtt

#### setCredentials

#### setSamplePeriod

#### setPostMethod

## FRAM_FM24CL64B

Esta clase es para el uso y configuración de una memoria FRAM, el constructor recibe como argumentos un semaforo al puerto i2c, y opcionalmente el puerto i2c a usar y la dirección i2c de la FRAM. Por defecto el puerto y la dirección son Wire y 0x50
La clase tiene además dos métodos para escribir y leer arrays desde la FRAM además de un método para probar la FRAM. 

## quectelEC

Esta clase maneja toda la comunicación del quectel. Al usar la librería preferences, no puede ser instanciada como un objeto global, pero si se puede instanciar como un puntero global al objeto para luego ser inicializado.
El constructor recibe como argumento el IO expander y un semaforo I2C.

Ejemplo de inicialización de GPRS:

``` C++
#include <Wire.h>
#include <PCA9535PW.h>
#include "quectelEC.hpp"

//i2c mutex
SemaphoreHandle_t i2cSemaphore;

quectelEC *gprs = NULL;
PCAL9535A::PCAL9535A<TwoWire> gpio(Wire);

deviceConfig *pulseConfiguration;

#define mqttUrl "zembia.cl"
#define mqttPort 1883
#define mqttTopic "zembia/1"
#define mqttUser "inye"
#define mqttPassword "daledale"

void setup()
{
    //I2C semaphore, used for every i2c object to avoid bus collisions
    if (i2cSemaphore == NULL) 
    {
        i2cSemaphore = xSemaphoreCreateMutex();
        if (i2cSemaphore != NULL) 
        {
            xSemaphoreGive(i2cSemaphore);
        }
    }

    //initializing i2c devices
    Wire.begin(SDA_PIN,SCL_PIN,I2C_SPEED);
    if (xSemaphoreTake(i2cSemaphore, (TickType_t)5) == pdTRUE) {
      gpio.begin();
      xSemaphoreGive(i2cSemaphore);
      //Trow an error
    }

    //Initialize pulse configuration
    pulseConfiguration = new deviceConfig();
    pulseConfiguration->configMqtt(mqttUrl,mqttTopic,mqttPort);
    pulseConfiguration->setCredentials(mqttUser,mqttPassword);

    //Initialize GPRS
    gprs = new quectelEC(gpio,i2cSemaphore); //Constructor needs I/O expander object and i2csemaphore
    gprs->setDeviceConfiguration(pulseConfiguration);
    gprs->powerUp();
}

void loop()
{
    if (gprs->isConnected)
    {
        gprs->postMqttMsg("Hello World!");
    }
    delay(15000);
}
```

