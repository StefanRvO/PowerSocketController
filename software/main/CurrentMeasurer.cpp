#include "CurrentMeasurer.h"
#include <cmath>
#define SAMPLE_FREQ_ADC 5000

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
    #include "soc/rtc_io_reg.h"
    #include "soc/soc.h"
    #include "soc/sens_reg.h"

}
#include <algorithm>

//We can not access flash memory while in the timer interrupt function.
//The below functions reimplement the timer and adc interfaces with IRAM_ATTR so thay can be accessed
//Without flash access.

//We can apparently not do floating point operations  in flash restructed interrupts, so do the computation here
static const DRAM_ATTR uint32_t interval = TIMER_INTERVAL0_SEC_ADC * TIMER_SCALE_ADC;
extern portMUX_TYPE rtc_spinlock; //Defined in rtc_module.c


esp_err_t IRAM_ATTR timer0_get_counter_value_iram(timer_idx_t timer_num, uint64_t* timer_val)
{
    TIMERG0.hw_timer[timer_num].update = 1;
    *timer_val = ((uint64_t) TIMERG0.hw_timer[timer_num].cnt_high << 32)
        | (TIMERG0.hw_timer[timer_num].cnt_low);
    return ESP_OK;
}

esp_err_t IRAM_ATTR timer0_get_alarm_value_iram(timer_idx_t timer_num, uint64_t* alarm_value)
{
    *alarm_value = ((uint64_t) TIMERG0.hw_timer[timer_num].alarm_high << 32)
                | (TIMERG0.hw_timer[timer_num].alarm_low);
    return ESP_OK;
}

esp_err_t IRAM_ATTR timer0_set_alarm_iram(timer_idx_t timer_num, timer_alarm_t alarm_en)
{
    TIMERG0.hw_timer[timer_num].config.alarm_en = alarm_en;
    return ESP_OK;
}

esp_err_t IRAM_ATTR timer0_set_alarm_value_iram(timer_idx_t timer_num, uint64_t alarm_value)
{
    TIMERG0.hw_timer[timer_num].alarm_high = (uint32_t) (alarm_value >> 32);
    TIMERG0.hw_timer[timer_num].alarm_low = (uint32_t) alarm_value;
    return ESP_OK;
}




int IRAM_ATTR adc1_get_voltage_iram(adc1_channel_t channel)
{ //Here we need to implement the adc reading. mainly just copy from the official implementation.
  //Only difference is that RTC_MODULE_CHECK is not called. Really not important, just don't give incorrect channel numbers.
    uint16_t adc_value;

    portENTER_CRITICAL(&rtc_spinlock);
    //Adc Controler is Rtc module,not ulp coprocessor
    SET_PERI_REG_BITS(SENS_SAR_MEAS_START1_REG, 1, 1, SENS_MEAS1_START_FORCE_S); //force pad mux and force start
    //Bit1=0:Fsm  Bit1=1(Bit0=0:PownDown Bit10=1:Powerup)
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR, 0, SENS_FORCE_XPD_SAR_S); //force XPD_SAR=0, use XPD_FSM
    //Disable Amp Bit1=0:Fsm  Bit1=1(Bit0=0:PownDown Bit10=1:Powerup)
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_AMP, 0x2, SENS_FORCE_XPD_AMP_S); //force XPD_AMP=0
    //Open the ADC1 Data port Not ulp coprocessor
    SET_PERI_REG_BITS(SENS_SAR_MEAS_START1_REG, 1, 1, SENS_SAR1_EN_PAD_FORCE_S); //open the ADC1 data port
    //Select channel
    SET_PERI_REG_BITS(SENS_SAR_MEAS_START1_REG, SENS_SAR1_EN_PAD, (1 << channel), SENS_SAR1_EN_PAD_S); //pad enable
    SET_PERI_REG_BITS(SENS_SAR_MEAS_CTRL_REG, 0xfff, 0x0, SENS_AMP_RST_FB_FSM_S);  //[11:8]:short ref ground, [7:4]:short ref, [3:0]:rst fb
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT1_REG, SENS_SAR_AMP_WAIT1, 0x1, SENS_SAR_AMP_WAIT1_S);
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT1_REG, SENS_SAR_AMP_WAIT2, 0x1, SENS_SAR_AMP_WAIT2_S);
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_SAR_AMP_WAIT3, 0x1, SENS_SAR_AMP_WAIT3_S);
    while (GET_PERI_REG_BITS2(SENS_SAR_SLAVE_ADDR1_REG, 0x7, SENS_MEAS_STATUS_S) != 0); //wait det_fsm==0
    SET_PERI_REG_BITS(SENS_SAR_MEAS_START1_REG, 1, 0, SENS_MEAS1_START_SAR_S); //start force 0
    SET_PERI_REG_BITS(SENS_SAR_MEAS_START1_REG, 1, 1, SENS_MEAS1_START_SAR_S); //start force 1
    while (GET_PERI_REG_MASK(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_DONE_SAR) == 0) {}; //read done
    adc_value = GET_PERI_REG_BITS2(SENS_SAR_MEAS_START1_REG, SENS_MEAS1_DATA_SAR, SENS_MEAS1_DATA_SAR_S);
    portEXIT_CRITICAL(&rtc_spinlock);

    return adc_value;}


void IRAM_ATTR CurrentMeasurer::current_timer_intr( void *arg)
{
    TIMERG0.int_clr_timers.t1 = 1;
    amp_measurement measurement;
    CurrentMeasurer *cur_measurer = (CurrentMeasurer *)arg;
    uint64_t timer_val;
    for(uint8_t i = 0; i < cur_measurer->pin_num; i++)
    {

        measurement.raw_sample = adc1_get_voltage_iram(cur_measurer->pins[i]);
        timer0_get_counter_value_iram(TIMER_NUM_ADC, &timer_val);
        measurement.period = timer_val - cur_measurer->last_time[i];
        if (xQueueSendToBackFromISR(cur_measurer->adc_queues[i], &measurement, NULL) != pdPASS)
            continue;

        cur_measurer->last_time[i] = timer_val;
    }
    uint64_t cur_alarm = 0;
    timer0_get_alarm_value_iram(TIMER_NUM_ADC, &cur_alarm);
    timer0_get_counter_value_iram(TIMER_NUM_ADC, &timer_val);
    cur_alarm += interval;
    if(timer_val > cur_alarm) cur_alarm = timer_val + interval;
    timer0_set_alarm_value_iram(TIMER_NUM_ADC, cur_alarm);
    timer0_set_alarm_iram(TIMER_NUM_ADC, TIMER_ALARM_EN);
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

    xTaskCreatePinnedToCore(CurrentMeasurer::sample_thread_wrapper, "ADC_Sampler", 6000, this, 10, NULL, 1);
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
    ESP_ERROR_CHECK(timer_isr_register(TIMER_GROUP_ADC, TIMER_NUM_ADC, &CurrentMeasurer::current_timer_intr, (void *)this, ESP_INTR_FLAG_IRAM, NULL));
    ESP_ERROR_CHECK(timer_start(TIMER_GROUP_ADC, TIMER_NUM_ADC));

    printf("entered sampler, samle interval: %f\n", TIMER_INTERVAL0_SEC_ADC * TIMER_SCALE_ADC);
    amp_measurement cur_sample;

    while(1)
    {
        //We wait untill the queue is filled up 1/6, so we can process them in chunks.
        //Without during this, everything may grind to a complete halt!
        //May need to be tunned a bit to optimise performance
        while(uxQueueMessagesWaiting(this->adc_queues[0]) < QUEUE_SIZE / 6)
        {
            vTaskDelay( ( (TIMER_INTERVAL0_SEC_ADC * QUEUE_SIZE * 1000) / 6. ) / portTICK_PERIOD_MS);
            printf("%u\n",uxQueueMessagesWaiting(this->adc_queues[0]));

        }
        for(uint8_t i = 0; i < this->pin_num; i++)
        {
            //Grab the current state of the switch. This will cause inaccuracies when switching on and off,
            //As this does not take delay in the queue into account, but hopefully this is ok.
            switch_state this_state = this->switch_handler->get_switch_state(i);

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
    if(cur_calibration.bias_off.last.bias == 0) cur_calibration.conversion = 0;
    else cur_calibration.conversion = 5. / cur_calibration.bias_off.last.bias;
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
    int64_t *time;
    bool *completed;
    if(bias_type == on)
    {
        bias = &cur_calibration.bias_on.last.bias;
        stddev = &cur_calibration.bias_on.last.stddev;
        time = &cur_calibration.bias_on.time;
        completed = &cur_calibration.bias_on.last.completed;
    }
    else
    {
        bias = &cur_calibration.bias_off.last.bias;
        stddev = &cur_calibration.bias_off.last.stddev;
        time = &cur_calibration.bias_off.time;
        completed = &cur_calibration.bias_off.last.completed;

    }
    if(cur_stats.cnt == 0)
    {
        this->switch_handler->set_switch_state(channel, bias_type, false);
        //Clear the queue so we only get samples for when the switch was on/off.
        xQueueReset(this->adc_queues[channel]);
        *time = -TIMER_SCALE_ADC * 2; //Wait until the relay have been on/off in 2 seconds
    }
    else if(*time < 0)
    {
        *time += cur_sample.period;
        if(cur_stats.time >= 0) //Set time to excatly zero when done waiting
        {
            *time = 0;
            *bias = 0;
            *stddev = 0;
            *completed = false;
        }
    }
    else if(*time >= 0)
    {   //Compute mean and variance using https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Weighted_incremental_algorithm
        //We could in theory run this algorithm "always" to update on - the -go...
        *time += cur_sample.period;
        double mean_old = *bias;
        *bias = mean_old + (cur_sample.period / double(*time) ) * (cur_sample.raw_sample - mean_old);
        *stddev += cur_sample.period * (cur_sample.raw_sample - mean_old) * (cur_sample.raw_sample - *bias);

        if(*time >= TIMER_SCALE_ADC * 5 - interval / 2) //Exit after 5 seconds +- interval / 2 to get the best fit.
        {
            *stddev /= *time;
            *stddev = sqrt(*stddev);
            printf("channel:%hhu\tbias:%f\tstddev:%f\ttime: %llu\n", channel, *bias, *stddev, *time);
            *time = 0;
            cur_stats.cnt = 0;
            *completed = true;
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
        amps = ( ( (int32_t)cur_sample.raw_sample) - cur_calibration.bias_on.last.bias) * cur_calibration.conversion;
    else if(current_state == off)
        amps = ( ( (int32_t)cur_sample.raw_sample) - cur_calibration.bias_off.last.bias) * cur_calibration.conversion;
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
calibration_result CurrentMeasurer::handle_live_calib(uint8_t &channel, amp_measurement &cur_sample, switch_state &current_state)
{
    CurrentCalibration &cur_calibration = this->cur_calibs[channel];
    CurrentStatistics &cur_stats = this->cur_statistics[channel];
    BiasCalibrationList *bias_list;
    if(current_state == on)
    {
        bias_list = &cur_calibration.bias_on;
    }
    else
    {
        bias_list = &cur_calibration.bias_off;
    }
    //Update bias and stddev
    bias_list->time += cur_sample.period;
    double mean_old = bias_list->next.bias;
    bias_list->next.bias = mean_old + (cur_sample.period / double(bias_list->time) ) * (cur_sample.raw_sample - mean_old);
    bias_list->next.stddev += cur_sample.period * (cur_sample.raw_sample - mean_old) * (cur_sample.raw_sample - bias_list->next.bias);

    //Check if it is time to swap (we should probably time this with rms update).
    if(bias_list->time >= TIMER_SCALE_ADC * CURCALIB_SWAP_TIME - interval / 2) //Exit after 5 seconds +- interval / 2 to get the best fit.
    {
        bias_list->next.stddev /= bias_list->time;
        bias_list->time = 0;
        bias_list->next.completed = true;
        std::swap(bias_list->next, bias_list->last);
        bias_list->next.completed = false;
        bias_list->next.stddev = 0;
        bias_list->next.bias = 0;
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
