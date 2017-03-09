#pragma once
//This class handles the control of the different switches, saving the state of the switches to NVS storage when changed, as well as retrieving them when starting up.
#include "driver/gpio.h"
#include "SettingsHandler.h"

enum switch_state : uint8_t
{
    off = 0,
    on,
    invalid,
};

class SwitchHandler;

class SwitchHandler
{
    public:
        static SwitchHandler *get_instance(const  gpio_num_t *_relay_pins, const gpio_num_t *_button_pins, const gpio_num_t *_button_leds, size_t _pin_num);
        static SwitchHandler *get_instance();

        ~SwitchHandler();
        void set_switch_state(uint8_t switch_num, switch_state state, bool write_to_nvs = true);
        switch_state get_switch_state(uint8_t switch_num);
        uint8_t get_switch_count();
    private:
        static SwitchHandler *instance;
        SwitchHandler(const  gpio_num_t *_relay_pins, const gpio_num_t *_button_pins, const gpio_num_t *_button_leds, size_t _pin_num);
        const gpio_num_t *relay_pins;
        const gpio_num_t *button_pins;
        const gpio_num_t *button_leds;
        size_t pin_num;
        SettingsHandler *s_handler;
        void setup_relay_pins();
        void setup_button_pins();
        void setup_button_leds();
};
