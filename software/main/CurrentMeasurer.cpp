#include "CurrentMeasurer.h"

extern "C"
{
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "stdio.h"
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
    xTaskCreate(CurrentMeasurer::sample_thread_wrapper, "ADC_Sampler", 4000, this, 10, NULL);
}

void CurrentMeasurer::sample_thread_wrapper(void *PvParameters)
{
    ((CurrentMeasurer *)PvParameters)->sample_thread();
}
void CurrentMeasurer::sample_thread()
{
    printf("entered sampler\n");
    while(1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        double average = 0;
        for(uint j = 0; j < 20000; j++ )
            for(uint32_t i = 0; i < this->pin_num; i++)
            {
                average += adc1_get_voltage(this->pins[i]);
            }
            printf("ADC1,6: %f\n", average / 20000.);

    }
}
