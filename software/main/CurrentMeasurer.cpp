#include "CurrentMeasurer.h"
#include <cmath>
#define SAMPLE_FREQ_ADC 2500

#define TIMER_GROUP_ADC TIMER_GROUP_0
#define TIMER_NUM_ADC TIMER_1

#define TIMER_DIVIDER_ADC 2
#define TIMER_SCALE_ADC    (TIMER_BASE_CLK / TIMER_DIVIDER_ADC)  /*!< used to calculate counter value */
#define TIMER_FINE_ADJ_ADC   (1.4*(TIMER_BASE_CLK / TIMER_DIVIDER_ADC)/1000000) /*!< used to compensate alarm value */
#define TIMER_INTERVAL0_SEC_ADC (1. / SAMPLE_FREQ_ADC) /*!< test interval for timer 0 */

#define QUEUE_SIZE 1000 //We can probably reduce this to about 100 (depending on sample frequency)

extern "C"
{
    #include "stdio.h"
    #include "esp_system.h"
    #include "esp_err.h"
    #include "math.h"
}

//We can not access flash memory while in this function. This either needs to be fixed,
//Or we need a lock. It will potentially create unacceptable slowdowns with a lock,
//As e.g. the webserver highly depends on the flash access, and will probably crawl to a halt
//With this restriction. It will also be un-nice to manage this restriction

void CurrentMeasurer::current_timer_intr(__attribute__((unused)) void *arg)
{
    TIMERG0.int_clr_timers.t1 = 1;
    amp_measurement measurement;
    CurrentMeasurer *cur_measurer = (CurrentMeasurer *)arg;
    uint64_t timer_val;
    for(uint8_t i = 0; i < cur_measurer->pin_num; i++)
    {

        measurement.raw_sample = adc1_get_voltage(cur_measurer->pins[i]);
        timer_get_counter_value(TIMER_GROUP_ADC, TIMER_NUM_ADC, &timer_val);
        measurement.period = timer_val - cur_measurer->last_time[i];
        if (xQueueSendToBackFromISR(cur_measurer->adc_queues[i], &measurement, NULL) != pdPASS)
            continue;

        cur_measurer->last_time[i] = timer_val;
    }
    uint64_t cur_alarm = 0;
    ESP_ERROR_CHECK(timer_get_alarm_value(TIMER_GROUP_ADC, TIMER_NUM_ADC, &cur_alarm));
    cur_alarm += TIMER_INTERVAL0_SEC_ADC * TIMER_SCALE_ADC;
    ESP_ERROR_CHECK(timer_set_alarm_value(TIMER_GROUP_ADC, TIMER_NUM_ADC, cur_alarm));
    ESP_ERROR_CHECK(timer_set_alarm(TIMER_GROUP_ADC, TIMER_NUM_ADC, TIMER_ALARM_EN));
    return;
}

CurrentMeasurer *CurrentMeasurer::instance = nullptr;


CurrentMeasurer *CurrentMeasurer::get_instance()
{
    assert(CurrentMeasurer::instance != nullptr);
    return CurrentMeasurer::instance;
}

CurrentMeasurer *CurrentMeasurer::get_instance(const adc1_channel_t *_pins, size_t _pin_num)
{
    if(CurrentMeasurer::instance == nullptr)
    {
        CurrentMeasurer::instance = new CurrentMeasurer(_pins, _pin_num);
    }
    return CurrentMeasurer::instance;
}

CurrentMeasurer::CurrentMeasurer(const adc1_channel_t *_pins, size_t _pin_num)
: pins(_pins), pin_num(_pin_num), switch_handler(SwitchHandler::get_instance()),
  settings_handler(SettingsHandler::get_instance())
{
    this->adc_queues = new QueueHandle_t[this->pin_num];
    this->last_time = new uint64_t[this->pin_num];
    this->cur_calibs = new CurrentCalibration[this->pin_num];
    this->cur_statistics = new CurrentStatistics[this->pin_num];
    this->cur_state = new CurrentSampleState[this->pin_num];
    //Setup adc settings

    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_12Bit));
    for(uint8_t i = 0; i < this->pin_num; i++)
    {
        //Setup queues for the adc sampling. We allocate space for 1K samples.
        this->adc_queues[i] = xQueueCreate(QUEUE_SIZE, sizeof(amp_measurement));
        this->last_time[i] = 0;
        if(this->load_current_calibration(i, this->cur_calibs[i], true) == success)
             this->cur_state[i] = calibration_done;
        else this->cur_state[i] = calibration_start;

        //Set amplification for adc
        ESP_ERROR_CHECK(adc1_config_channel_atten(this->pins[i], ADC_ATTEN_11db));
    }

    xTaskCreatePinnedToCore(CurrentMeasurer::sample_thread_wrapper, "ADC_Sampler", 4000, this, 10, NULL, 1);
}

void CurrentMeasurer::sample_thread_wrapper(void *PvParameters)
{
    ((CurrentMeasurer *)PvParameters)->sample_thread();
}
void CurrentMeasurer::sample_thread()
{
    //Setup timer for sampling
    timer_config_t timer_config;
    timer_config.alarm_en = true;
    timer_config.counter_en = TIMER_PAUSE;
    timer_config.intr_type = TIMER_INTR_LEVEL;
    timer_config.counter_dir = TIMER_COUNT_UP;
    timer_config.auto_reload = false;
    timer_config.divider = TIMER_DIVIDER_ADC;
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_ADC, TIMER_NUM_ADC, &timer_config));
    ESP_ERROR_CHECK(timer_pause(TIMER_GROUP_ADC, TIMER_NUM_ADC));
    ESP_ERROR_CHECK(timer_set_counter_value(TIMER_GROUP_ADC, TIMER_NUM_ADC, 0x00000000ULL));
    ESP_ERROR_CHECK(timer_set_alarm_value(TIMER_GROUP_ADC, TIMER_NUM_ADC, TIMER_INTERVAL0_SEC_ADC * TIMER_SCALE_ADC));
    ESP_ERROR_CHECK(timer_enable_intr(TIMER_GROUP_ADC, TIMER_NUM_ADC));
    ESP_ERROR_CHECK(timer_isr_register(TIMER_GROUP_ADC, TIMER_NUM_ADC, &CurrentMeasurer::current_timer_intr, (void *)this, 0, NULL));
    ESP_ERROR_CHECK(timer_start(TIMER_GROUP_ADC, TIMER_NUM_ADC));

    printf("entered sampler\n");
    amp_measurement cur_sample;

    while(1)
    {
        //We wait untill the queue is filled up 1/6, so we can process them in chunks.
        //Without during this, everything may grind to a complete halt!
        //May need to be tunned a bit to optimise performance
        while(uxQueueMessagesWaiting(this->adc_queues[0]) < QUEUE_SIZE / 6)
            vTaskDelay( ( (TIMER_INTERVAL0_SEC_ADC * QUEUE_SIZE * 1000) / 6. ) / portTICK_PERIOD_MS);

        for(uint8_t i = 0; i < this->pin_num; i++)
        {
            //Grab the current state of the switch. This will cause inaccuracies when switching on and off,
            //As this does not take delay in the queue into account, but hopefully this is ok.
            switch_state this_state = this->switch_handler->get_switch_state(i);

            //printf("%u\t%u\n", i, uxQueueMessagesWaiting(this->adc_queues[i]));
            while(xQueueReceive(this->adc_queues[i], &cur_sample, 0) == pdTRUE)
            {
                switch(this->cur_state[i])
                {
                    //We now have the sample in cur_sample
                    case calibration_start:
                        this->handle_calibration_start(i, cur_sample);
                        break;
                    case calibrating_bias_on:
                        this->handle_bias_on_calibration(i, cur_sample);
                        break;
                    case calibrating_bias_off:
                        this->handle_bias_off_calibration(i, cur_sample);
                        break;
                    case calibrating_conversion:
                        this->handle_conversion_calibration(i, cur_sample);
                        break;
                    case calibration_done:
                        this->handle_calibration_done(i, cur_sample);
                        break;
                    case measuring:
                        this->handle_measuring(i, cur_sample, this_state);
                        break;

                }
            }
            if(i == 2)
                printf("plug: %u\t RMS: %fA\n", i, this->cur_statistics[i].rms_current);

        }
    }
}


calibration_result CurrentMeasurer::handle_conversion_calibration(uint8_t &channel, amp_measurement &cur_sample)
{   //We calculate the conversion rate as follows:
    //We assume a magnetic offset of the sensors of 0mV. In reality, the datasheet states +-40mV, resulting in an inaccuracy of about +-1%
    //As the sensors measure +-5A, the conversion rate will be 5 / bias_off.
    printf("Conversion calibration.\n");

    __attribute__((unused)) CurrentCalibration &cur_calibration = this->cur_calibs[channel];
    __attribute__((unused)) CurrentStatistics &cur_stats = this->cur_statistics[channel];
    if(cur_calibration.bias_off == 0) cur_calibration.conversion = 0;
    else cur_calibration.conversion = 5. / cur_calibration.bias_off;
    this->cur_state[channel] = calibration_done;
    return success;
}

calibration_result CurrentMeasurer::handle_calibration_done(uint8_t &channel, amp_measurement &cur_sample)
{
    //This is the last calibration for now. Set switch states to the saved one and continue to measuring.
    //Also reset the stats
    //Also, save the calibration to nvs.
    printf("Calibration done\n");
    this->switch_handler->set_saved_state(channel);
    this->cur_calibs[channel].completed = true;
    this->save_current_calibration(channel, this->cur_calibs[channel]);
    xQueueReset(this->adc_queues[channel]);
    this->cur_statistics[channel] = CurrentStatistics();
    this->cur_state[channel] = measuring;
    return success;
}

calibration_result CurrentMeasurer::handle_calibration_start(uint8_t &channel, amp_measurement &cur_sample)
{   //Reset the calibration and statistics and clear the queue
    //(this may cause issues for individual calibrations as we only pool on how "full" the queue for channel 0 is.).
    printf("Starting ACS712 Calibration\n");

    CurrentCalibration &cur_calibration = this->cur_calibs[channel];
    CurrentStatistics &cur_stats = this->cur_statistics[channel];
    xQueueReset(this->adc_queues[channel]);
    this->cur_state[channel] = calibrating_bias_on;
    cur_calibration = CurrentCalibration();
    cur_stats = CurrentStatistics();
    return success;
}


calibration_result CurrentMeasurer::handle_bias_on_calibration(uint8_t &channel, amp_measurement &cur_sample)
{
    calibration_result result = this->handle_bias_calibration(channel, cur_sample, on);
    if(result == in_progress)
    {
    }
    else if(result == fail)
    { //We should probably handle this somehow :S, but right now the calibration can't even fail...
    }
    else if(result == success)
    {
        printf("finished bias on Calibration\n");
        //Continue to off calibration
        this->cur_state[channel] = calibrating_bias_off;
    }
    return result;
}

calibration_result CurrentMeasurer::handle_bias_off_calibration(uint8_t &channel, amp_measurement &cur_sample)
{
    calibration_result result = this->handle_bias_calibration(channel, cur_sample, off);
    if(result == in_progress)
    {
    }
    else if(result == fail)
    { //We should probably handle this somehow :S, but right now the calibration can't even fail...
    }
    else if(result == success)
    {
        printf("finished bias off Calibration\n");
        this->cur_state[channel] = calibrating_conversion;
    }
    return result;
}

calibration_result CurrentMeasurer::handle_bias_calibration(uint8_t &channel, amp_measurement &cur_sample, switch_state bias_type)
{
    CurrentCalibration &cur_calibration = this->cur_calibs[channel];
    CurrentStatistics &cur_stats = this->cur_statistics[channel];
    double *bias;
    double *stddev;
    if(bias_type == on)
    {
        bias = &cur_calibration.bias_on;
        stddev = &cur_calibration.stddev_on;

    }
    else
    {
        bias = &cur_calibration.bias_off;
        stddev = &cur_calibration.stddev_off;

    }
    if(cur_stats.cnt == 0)
    {
        this->switch_handler->set_switch_state(channel, bias_type, false);
        //Clear the queue so we only get samples for when the switch was on/off.
        xQueueReset(this->adc_queues[channel]);
        cur_stats.time = -TIMER_SCALE_ADC * 2; //Wait until the relay have been on/off in 2 seconds
    }
    else if(cur_stats.time < 0)
    {
        cur_stats.time += cur_sample.period;
        if(cur_stats.time >= 0) //Set time to excatly zero when done waiting
        {
            cur_stats.time = 0;
            *bias = 0;
            *stddev = 0;
        }
    }
    else if(cur_stats.time >= 0)
    {   //Compute mean and variance using https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Weighted_incremental_algorithm
        //We could in theory run this algorithm "always" to update on - the -go...
        cur_stats.time += cur_sample.period;
        double mean_old = *bias;
        *bias = mean_old + (cur_sample.period / double(cur_stats.time) ) * (cur_sample.raw_sample - mean_old);
        *stddev += cur_sample.period * (cur_sample.raw_sample - mean_old) * (cur_sample.raw_sample - *bias);

        if(cur_stats.time >= TIMER_SCALE_ADC * 5) //Exit after 5 seconds
        {
            *stddev /= cur_stats.time;
            printf("channel:%hhu\tbias:%f\tstddev:%f\ttime: %llu\n", channel, *bias, *stddev, cur_stats.time);
            cur_stats.time = 0;
            cur_stats.cnt = 0;
            return success;
        }
    }
    cur_stats.cnt++;
    return in_progress;

}

calibration_result CurrentMeasurer::handle_measuring(uint8_t &channel, amp_measurement &cur_sample, switch_state &current_state)
{
    CurrentCalibration &cur_calibration = this->cur_calibs[channel];
    CurrentStatistics &cur_stats = this->cur_statistics[channel];

    float amps;
    if(current_state == on)
        amps = ( ( (int32_t)cur_sample.raw_sample) - cur_calibration.bias_on) * cur_calibration.conversion;
    else if(current_state == off)
        amps = ( ( (int32_t)cur_sample.raw_sample) - cur_calibration.bias_off) * cur_calibration.conversion;
    else
        amps = 0;
    if(cur_stats.time >= TIMER_SCALE_ADC * 0.3)
    {
        //printf("%f\t%lld\n", cur_stats.squared_total, cur_stats.time);
        cur_stats.rms_current = sqrt(cur_stats.squared_total / cur_stats.time) ;
        cur_stats.squared_total = amps * amps * cur_sample.period;
        cur_stats.time = cur_sample.period;
    }
    else
    {
        cur_stats.squared_total += amps * amps * cur_sample.period;
        cur_stats.time += cur_sample.period;
    }

    return in_progress;
}

calibration_result CurrentMeasurer::load_current_calibration(uint8_t &channel, CurrentCalibration &calibration, bool from_nvs)
{
    if(from_nvs)
    {
        char *calib_str = (char *)malloc(10);
        snprintf(calib_str, 10,  "CCALIB%d", channel);
        size_t len = sizeof(CurrentCalibration);
        esp_err_t err = this->settings_handler->nvs_get(calib_str, &calibration, &len);
        free(calib_str);
        if(err != ESP_OK || len != sizeof(CurrentCalibration))
            return fail;
    }
    else
    {
        calibration = this->cur_calibs[channel];
    }
    return success;
}

void CurrentMeasurer::save_current_calibration(uint8_t &channel, CurrentCalibration &calibration)
{
    char *calib_str = (char *)malloc(10);
    snprintf(calib_str, 10,  "CCALIB%d", channel);
    ESP_ERROR_CHECK(this->settings_handler->nvs_set(calib_str, &calibration, sizeof(CurrentCalibration)));
    free(calib_str);
}

void CurrentMeasurer::recalib_current_sensors()
{
    for(uint8_t i = 0; i < this->pin_num; i++)
        this->recalib_current_sensor(i);
}

void CurrentMeasurer::recalib_current_sensor(uint8_t channel)
{
    this->cur_state[channel] = calibration_start;
}
