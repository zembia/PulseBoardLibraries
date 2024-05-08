#ifndef _SCHEDULLER_HPP
#define _SCHEDULLER_HPP
#include "Arduino.h"

class scheduller{
    public:
        scheduller(uint32_t ,void (*)(),uint32_t heapSize=1024);
    private:
        static void  _startTaskImpl(void* );
        void _timeLoop(void);
        void (*_callback)();
        TaskHandle_t    _timeLoopHandle;
        const char _logtag[12] = "schedduller";
        uint32_t _timePeriod;
};

#endif