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
//We use ADC1 for now.
//We may be able to expand to ADC2 at some future point with a bit of work.
//(NO, not possible! ADC2 uses wifi circuitry... (wtf!))
struct amp_measurement
{
    uint16_t raw_sample; //The value from the ADC
    uint32_t period; //The timer counts since last sample
    //We may need to expand with a timestamp depending on the reliability.
};

struct CurrentStatistics
//We may need to compute this in chunks (e.g. off 500ms intervals, as we actually needs all the samples to compute the rms.)
//This may be way way easier when we get the voltage too, as it is simply I * U *dt, and then add them up.
//take a snapshot at some interval and compare them, thus getting power consumption per period.
{
    float squared_total; //Squared total since last boot (how do we do rms in last x seconds without saving massive amounts of samples?)
};

struct CurrentCalibration
{
    float conversion;   //Multiply with this number to convert from adc values to current in amps.
                        //We can probably not calibrate this with the current hardware, so we need to measure it
                        //Depending on the voltage divider and the acs712 specs

    float bias_on;      //This is the "bias" while the relays are switched on. Can be easily measured, as it should simply be the
                        //average measurement. Unless hardware is used which draws current in very strange ways, e.g, only on the
                        //top half of the sine. We may need to look into if this is a problem, if then, just calibrate at first startup
                        //and save to nvs.

    float bias_off;     //The bias when the realays are off. May not be needed as we could asume "zero" current while off.
                        // but for safety purposes, it may be good to sanity check the current in the off state.

};

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
        QueueHandle_t *adc_queues;
        uint64_t *last_time;
};
