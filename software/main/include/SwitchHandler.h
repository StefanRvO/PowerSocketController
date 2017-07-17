#pragma once
//This class handles the control of the different switches, saving the state of the switches to NVS storage when changed, as well as retrieving them when starting up.
#include "driver/gpio.h"
#include "SettingsHandler.h"
#include "freertos/FreeRTOS.h"

#include "freertos/task.h"
#include "freertos/semphr.h"
#include "PCF8574_Handler.h"
#define DEBOUNCE_TIME 30 //Millisecond threshold for debouncing
#define DOUBLECLICK_THRESHOLD 500//Threshold time for doubleclicking
#define POLL_TIME 15
#define LEDC_BITWIDTH LEDC_TIMER_13_BIT
#define FADE_TIME 400
#define TIME_5V 200 //Number of milliseconds we set the relay voltage to 5V when switching on. Half will be before the switch, and helf after
#define TIME_5V_STEPS (TIME_5V / POLL_TIME) //Number of POLL_TIME for TIME_5V

extern "C"
{
    #include "freertos/FreeRTOS.h"

    #include <freertos/timers.h>
}

enum led_control_mode
{
    output = 0, //The LEDs is controlled by the state of the switches.
    blinking, //The LEDs are blinking. The blink speed is controlled by the member variable blink_time.
};

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
        static SwitchHandler *get_instance(const  PCF8574_pin *_relay_pins, const PCF8574_pin *_relay_voltage_pin,
                const PCF8574_pin *_button_pins, const gpio_num_t *_button_leds,
                 size_t _pin_num);
        static SwitchHandler *get_instance();

        ~SwitchHandler();
        void set_switch_state(uint8_t switch_num, switch_state state, bool write_to_nvs = true);
        switch_state get_switch_state(uint8_t switch_num);
        uint8_t get_switch_count();

    private:
        uint64_t blink_time = 20; //When the LED's are in blinking state, they will blink with a frequency of 1000/(2 * blink_time).
                                  //The resolution of this time will be POLL_TIME
        uint64_t blink_counter = 0; //Counter for performing the blinking.

        uint64_t led_state_timeout = 0; //Number of milliseconds untill the LED mode will be changed to output.
                                        //To keep the LED's in some state, this will need to be continuosly set or be set to a really high timeout.

        led_control_mode led_mode = output;
        static SwitchHandler *instance;
        SwitchHandler(const  PCF8574_pin *_relay_pins, const PCF8574_pin *_relay_voltage_pin,
                const PCF8574_pin *_button_pins, const gpio_num_t *_button_leds,
                 size_t _pin_num);

        const PCF8574_pin *relay_pins;
        const PCF8574_pin *relay_voltage_pin;
        const PCF8574_pin *button_pins;
        const gpio_num_t *button_leds;
        uint32_t relay_voltage_pin_timeout = TIME_5V_STEPS;
        xSemaphoreHandle set_mux;

        size_t pin_num;

        SettingsHandler *s_handler;
        TimerHandle_t button_poll_timer = nullptr;
        PCF8574_Handler *pcf8574;
        void setup_relay_pins();
        void setup_button_pins();
        void setup_button_leds();
        static void poll_buttons(TimerHandle_t xTimer); //poll each button individually.
                                                        //Afterwards, check for events which depends on combined states.
        button_event poll_button(uint8_t button_num, bool raw_state); //emit events on changes (debounced)
        void handle_event(button_event the_event, uint8_t button_num);
        void handle_button_states();
        void set_led_mode(led_control_mode mode, uint64_t timeout);
        void set_led_blink_time(uint64_t blink_time_);
        void handle_leds();
        button_state *button_states = nullptr;
        switch_state *state_buff = nullptr;
        switch_state *led_state_buff = nullptr;

        SemaphoreHandle_t led_settings_lock; //Lock for settings related to the LEDs

};
