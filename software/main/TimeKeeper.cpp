#include "TimeKeeper.h"
#include <stdio.h>
TimeKeeper *TimeKeeper::instance = nullptr;

TimeKeeper *TimeKeeper::get_instance()
{
    if(TimeKeeper::instance == nullptr)
    {
        TimeKeeper::instance = new TimeKeeper();
    }
    return TimeKeeper::instance;
}

void TimeKeeper::do_update()
{
    uint32_t current_tick_count = xTaskGetTickCount();
    xSemaphoreTake(this->lock, 100000 / portTICK_RATE_MS);
    if(current_tick_count < last_tick_count)
    {
        total_tick_count_wrapped += ( ((uint64_t)1) << 32);
    }
    last_tick_count = current_tick_count;
    xSemaphoreGive(this->lock);
}
TimeKeeper::TimeKeeper()
{
    this->last_tick_count = 0;
    this->total_tick_count_wrapped = 0;
    this->lock = xSemaphoreCreateMutex();
    if(this->lock == NULL) printf("error creating timekeeper semaphore!\n");
}

double TimeKeeper::get_uptime()
{
    //Do an update of the time.
    double cur_time = 0;
    this->do_update();
    xSemaphoreTake(this->lock, 100000 / portTICK_RATE_MS);
    cur_time = (total_tick_count_wrapped + last_tick_count) * double(portTICK_RATE_MS);
    xSemaphoreGive(this->lock);
    return cur_time / 1000.;
}

uint64_t TimeKeeper::get_uptime_milliseconds()
{
    //Do an update of the time.
    uint64_t cur_time = 0;
    this->do_update();
    xSemaphoreTake(this->lock, 100000 / portTICK_RATE_MS);
    cur_time = (total_tick_count_wrapped + last_tick_count) * portTICK_RATE_MS;
    xSemaphoreGive(this->lock);
    return cur_time;
}
