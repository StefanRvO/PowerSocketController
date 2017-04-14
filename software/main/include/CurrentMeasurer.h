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
#include "SwitchHandler.h"
#include "SettingsHandler.h"

#define BIAS_OFF_CALIB_TIME
#define BIAS_ON_CALIB_TIME

enum calibration_result
{
    in_progress,
    success,
    fail,
};
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
    float squared_total = 0; //Squared total since last boot (how do we do rms in last x seconds without saving massive amounts of samples?)
    uint32_t cnt = 0;
    int64_t time = 0;
    float rms_current = 0;
};

struct CurrentCalibration
{
    double conversion = 0;   //Multiply with this number to convert from adc values to current in amps.
                        //We can probably not calibrate this with the current hardware, so we need to measure it
                        //Depending on the voltage divider and the acs712 specs

    double stddev_on = 0;
    double bias_on = 0;      //This is the "bias" while the relays are switched on. Can be easily measured, as it should simply be the
                        //average measurement. Unless hardware is used which draws current in very strange ways, e.g, only on the
                        //top half of the sine. We may need to look into if this is a problem, if then, just calibrate at first startup
                        //and save to nvs.
    double stddev_off = 0;
    double bias_off = 0;     //The bias when the relays are off. May not be needed as we could asume "zero" current while off.
                        // but for safety purposes, it may be good to sanity check the current in the off state.
    bool completed = false;
};


enum CurrentSampleState
{
    calibration_start,
    calibrating_bias_on,
    calibrating_bias_off,
    calibrating_conversion, //Can we do this? how?
    calibration_done,
    measuring,
};

class CurrentMeasurer;

class CurrentMeasurer
{
    public:
        static CurrentMeasurer *get_instance();
        static CurrentMeasurer *get_instance(const adc1_channel_t *_pins, size_t _pin_num);
        calibration_result load_current_calibration(uint8_t &channel, CurrentCalibration &calibration, bool from_nvs = false);
        uint8_t get_current_count() {return this->pin_num;}
        void recalib_current_sensor(uint8_t channel);
        void recalib_current_sensors();

    private:
        calibration_result handle_conversion_calibration(uint8_t &channel, amp_measurement &cur_sample);
        calibration_result handle_bias_on_calibration(uint8_t &channel, amp_measurement &cur_sample);
        calibration_result handle_bias_off_calibration(uint8_t &channel, amp_measurement &cur_sample);
        calibration_result handle_measuring(uint8_t &channel, amp_measurement &cur_sample, switch_state &current_state);
        calibration_result handle_bias_calibration(uint8_t &channel, amp_measurement &cur_sample, switch_state bias_type);
        calibration_result handle_calibration_done(uint8_t &channel, amp_measurement &cur_sample);
        calibration_result handle_calibration_start(uint8_t &channel, amp_measurement &cur_sample);

        void save_current_calibration(uint8_t &channel, CurrentCalibration &calibration);
        CurrentMeasurer(const adc1_channel_t *_pins, size_t _pin_num);
        const adc1_channel_t *pins; //array of gpio pins to sample
        size_t pin_num; //number
        static CurrentMeasurer *instance;
        void static sample_thread_wrapper(void *PvParameters);
        void sample_thread();
        static void current_timer_intr(void *arg);
        QueueHandle_t *adc_queues = nullptr;
        uint64_t *last_time = nullptr;
        CurrentCalibration *cur_calibs = nullptr;
        CurrentStatistics *cur_statistics = nullptr;
        CurrentSampleState *cur_state = nullptr;
        SwitchHandler *switch_handler;
        SettingsHandler *settings_handler;
};
