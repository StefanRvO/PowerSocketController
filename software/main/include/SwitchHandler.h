#pragma once
//This class handles the control of the different switches, saving the state of the switches to NVS storage when changed, as well as retrieving them when starting up.
#include "driver/gpio.h"
#include "SettingsHandler.h"

#define DEBOUNCE_TIME 30 //Millisecond threshold for debouncing
#define DOUBLECLICK_THRESHOLD 500//Threshold time for doubleclicking
#define POLL_TIME 5
extern "C"
{
    #include "freertos/FreeRTOS.h"

    #include <freertos/timers.h>
}


enum switch_state : uint8_t
{
    off = 0,
    on,
    invalid,
};


enum button_push_state : uint8_t
{
    idle = 0,
    first_push,
    first_release,
    second_push,
    second_release,
};
enum button_event : uint8_t
{
    no_event = 0,
    single_push, //Emitted when transisting from idle to first_push state
    single_release, //Emitted when transisting from first_push to first_release
    double_push, //Emitted when transisting from first_release to second_push
    double_release //Emitted when transisting from second_push to second release
};

struct button_state
{
    button_push_state state = idle;
    bool raw_state = false; //State without debouncing and doubleclick.
    bool filtered_state = false;
    uint64_t timer = 0;  //The time in which the button has been in the current raw state.
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
        TimerHandle_t button_poll_timer = nullptr;
        void setup_relay_pins();
        void setup_button_pins();
        void setup_button_leds();
        static void poll_buttons(TimerHandle_t xTimer); //poll each button individually.
                                                        //Afterwards, check for events which depends on combined states.
        button_event poll_button(uint8_t button_num); //emit events on changes (debounced)
        void handle_event(button_event the_event, uint8_t button_num);
        void handle_button_states();
        button_state *button_states = nullptr;
        switch_state *state_buff = nullptr;
};
