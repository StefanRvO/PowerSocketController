#pragma once
//This class handles the control of the different switches, saving the state of the switches to NVS storage when changed, as well as retrieving them when starting up.
#include "driver/gpio.h"
class SwitchHandler;

class SwitchHandler
{
    public:
        static SwitchHandler *get_instance(const  gpio_num_t *_relay_pins, const gpio_num_t *_button_pins, const gpio_num_t *_button_leds, size_t _pin_num);
        static SwitchHandler *get_instance();

        ~SwitchHandler();

    private:
        static SwitchHandler *instance;
        SwitchHandler(const  gpio_num_t *_relay_pins, const gpio_num_t *_button_pins, const gpio_num_t *_button_leds, size_t _pin_num);
        const gpio_num_t *relay_pins;
        const gpio_num_t *button_pins;
        const gpio_num_t *button_leds;
        size_t pin_num;
};
