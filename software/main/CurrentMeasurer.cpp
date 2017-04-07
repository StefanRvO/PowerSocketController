#include "CurrentMeasurer.h"

#define SAMPLE_FREQ_ADC 2500

#define TIMER_GROUP_ADC TIMER_GROUP_0
#define TIMER_NUM_ADC TIMER_1

#define TIMER_DIVIDER_ADC 2
#define TIMER_SCALE_ADC    (TIMER_BASE_CLK / TIMER_DIVIDER_ADC)  /*!< used to calculate counter value */
#define TIMER_FINE_ADJ_ADC   (1.4*(TIMER_BASE_CLK / TIMER_DIVIDER_ADC)/1000000) /*!< used to compensate alarm value */
#define TIMER_INTERVAL0_SEC_ADC (1. / SAMPLE_FREQ_ADC) /*!< test interval for timer 0 */

#define QUEUE_SIZE 1000 //We can probably reduce this to about 100

extern "C"
{
    #include "stdio.h"
    #include "esp_system.h"
    #include "esp_err.h"
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
: pins(_pins), pin_num(_pin_num)
{
    this->adc_lock = xSemaphoreCreateMutex();
    this->adc_queues = (QueueHandle_t *)malloc(sizeof(QueueHandle_t) * this->pin_num);
    this->last_time = (uint64_t *)malloc(sizeof(uint64_t) * this->pin_num);
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_12Bit));
    for(uint8_t i = 0; i < this->pin_num; i++)
    {
        //Setup queues for the adc sampling. We allocate space for 1K samples.
        this->adc_queues[i] = xQueueCreate(QUEUE_SIZE, sizeof(amp_measurement));
        this->last_time[i] = 0;
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
    //vTaskDelay(1000 / portTICK_PERIOD_MS);

    //Setup adc settings
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
            //printf("%u\t%u\n", i, uxQueueMessagesWaiting(this->adc_queues[i]));
            uint16_t min = 0xFFFF;
            uint16_t max = 0;
            double total = 0;
            uint32_t counter = 0;

            while(xQueueReceive(this->adc_queues[i], &cur_sample, 0) == pdTRUE)
            {
                if(cur_sample.raw_sample > max) max = cur_sample.raw_sample;
                if(cur_sample.raw_sample < min) min = cur_sample.raw_sample;
                total += cur_sample.raw_sample;
                counter++;
                //We now have the sample in cur_sample
            }
            if(i == 2)
                printf("plug: %u\t max: %u\t min: %u\t cnt: %u \tmean: %f\n",  i, max, min, counter, total / counter);

        }
    }
}
