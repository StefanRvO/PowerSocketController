#pragma once

extern "C"
{
    #include "freertos/FreeRTOS.h"

    #include "driver/adc.h"
    #include "stdlib.h"
    #include "esp_err.h"
    #include "driver/timer.h"
    #include "freertos/task.h"
    #include "freertos/semphr.h"

}
//We use ADC1 for now. May be able to expand to ADC2 at some future point with a bit of work.
class CurrentMeasurer;

class CurrentMeasurer
{
    public:
        static CurrentMeasurer *get_instance();
        static CurrentMeasurer *get_instance(const adc1_channel_t *_pins, size_t _pin_num);

    private:
        CurrentMeasurer(const adc1_channel_t *_pins, size_t _pin_num);
        const adc1_channel_t *pins; //array of gpio pins to sample
        size_t pin_num; //number
        static CurrentMeasurer *instance;
        void static sample_thread_wrapper(void *PvParameters);
        void sample_thread();
        static void current_timer_intr(void *arg);
        SemaphoreHandle_t adc_lock; //Lock for settings related to the LEDs


};
