#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


//Class used to keep track of current uptime

class TimeKeeper;

class TimeKeeper
{
    public:
        static TimeKeeper *get_instance();
        void do_update(); //updates the wrapped counter and last_tick_count.
        double get_uptime(); //get time since boot.
        uint64_t get_uptime_milliseconds(); //get time since boot.

    private:
        static TimeKeeper *instance;
        TimeKeeper();
        uint64_t total_tick_count_wrapped;
        uint64_t last_tick_count;
        SemaphoreHandle_t lock;
};
