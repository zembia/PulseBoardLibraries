#include "scheduller.hpp"
#include "time.h"

scheduller::scheduller(uint32_t timePeriod, void (*callbackFunction)(),uint32_t heapSize)
{
    if (_timePeriod<=0)
    {
        ESP_LOGE("scheduller","INVALID TIME PERIOD");
        return;
    }
    _timePeriod = timePeriod*60;
    //_timePeriod = 10;
    _callback = callbackFunction;
    xTaskCreate(
        this->_startTaskImpl,
        "timeLoopTask",
        heapSize,
        this,
        4,
        &_timeLoopHandle
    );
}

void scheduller::_startTaskImpl(void *_this)
{
    ESP_LOGD("scheduller","CREARTING TIMELOOP TASK");
    static_cast<scheduller*>(_this)->_timeLoop();
}



void scheduller::_timeLoop(void)
{
    struct tm timeinfo;
    time_t lastEventTime;
    time_t currentTime;
    getLocalTime(&timeinfo);
    lastEventTime = mktime(&timeinfo);
    while(1){
        vTaskDelay(pdMS_TO_TICKS(500));

        getLocalTime(&timeinfo);
        currentTime = mktime(&timeinfo);

        //This avoid making the same call multiple times
        if (currentTime == lastEventTime)
        {
            continue;
        }
        
        if (_callback == NULL)
        {
            continue;
        }

        if (currentTime%_timePeriod == 0)
        {
            lastEventTime = currentTime;
            _callback();
        }
    }
}
