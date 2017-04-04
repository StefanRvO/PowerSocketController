#include "CurrentMeasurer.h"

#define SAMPLE_FREQ 10000
#define TIMER_GROUP TIMER_GROUP_0
#define TIMER_NUM TIMER_1
#define TIMER_DIVIDER 16
#define TIMER_SCALE    (TIMER_BASE_CLK / TIMER_DIVIDER)  /*!< used to calculate counter value */
#define TIMER_FINE_ADJ   (1.4*(TIMER_BASE_CLK / TIMER_DIVIDER)/1000000) /*!< used to compensate alarm value */
#define TIMER_INTERVAL0_SEC (1. / SAMPLE_FREQ) /*!< test interval for timer 0 */

extern "C"
{
    #include "stdio.h"
}

double adc_avg[3] = {0,0,0};
uint64_t cnt = 0;

//We can not access flash memory while in this function. This either needs to be fixed,
//Or we need a lock. It will potentially create unacceptable slowdowns with a lock,
//As e.g. the webserver highly depends on the flash access, and will probably crawl to a halt
//With this restriction. It will also be un-nice to manage this restriction
void CurrentMeasurer::current_timer_intr(__attribute__((unused)) void *arg)
{

    CurrentMeasurer *cur_measurer = (CurrentMeasurer *)arg;
    /*if(xSemaphoreTakeFromISR(cur_measurer->adc_lock, pdFALSE))
    {*/

        for(uint8_t i = 0; i < cur_measurer->pin_num; i++)
            adc_avg[i] = adc1_get_voltage(cur_measurer->pins[i]);

        //    i = i;
        cnt++;
        /*xSemaphoreGiveFromISR(cur_measurer->adc_lock, NULL);
    }*/
    TIMERG0.hw_timer[TIMER_NUM].update = 1;
    TIMERG0.int_clr_timers.t1 = 1;
    TIMERG0.hw_timer[TIMER_NUM].config.alarm_en = 1;
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
    xTaskCreatePinnedToCore(CurrentMeasurer::sample_thread_wrapper, "ADC_Sampler", 4000, this, 10, NULL, 0);
}

void CurrentMeasurer::sample_thread_wrapper(void *PvParameters)
{
    ((CurrentMeasurer *)PvParameters)->sample_thread();
}
void CurrentMeasurer::sample_thread()
{
    //Setup adc settings
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_12Bit));
    for(uint8_t i = 0; i < this->pin_num; i++)
        ESP_ERROR_CHECK(adc1_config_channel_atten(this->pins[i], ADC_ATTEN_11db));

    //Setup timer
    timer_config_t timer_config;
    timer_config.alarm_en = true;
    timer_config.counter_en = TIMER_PAUSE;
    timer_config.intr_type = TIMER_INTR_LEVEL;
    timer_config.counter_dir = TIMER_COUNT_UP;
    timer_config.auto_reload = true;
    timer_config.divider = TIMER_DIVIDER;
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP, TIMER_NUM, &timer_config));
    ESP_ERROR_CHECK(timer_pause(TIMER_GROUP, TIMER_NUM));
    ESP_ERROR_CHECK(timer_set_counter_value(TIMER_GROUP, TIMER_NUM, 0x00000000ULL));
    ESP_ERROR_CHECK(timer_set_alarm_value(TIMER_GROUP, TIMER_NUM, TIMER_INTERVAL0_SEC * TIMER_SCALE));
    ESP_ERROR_CHECK(timer_enable_intr(TIMER_GROUP, TIMER_NUM));
    ESP_ERROR_CHECK(timer_isr_register(TIMER_GROUP, TIMER_NUM, &CurrentMeasurer::current_timer_intr, (void *)this, 0, NULL));
    ESP_ERROR_CHECK(timer_start(TIMER_GROUP, TIMER_NUM));

    printf("entered sampler\n");
    while(1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        printf("%llu\t%f\n", cnt, adc_avg[2] );
    }
}
