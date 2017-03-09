#include "SwitchHandler.h"
extern "C"
{
    #include "stdlib.h"
}

__attribute__((unused)) static const char *TAG = "SwitchHandler";

SwitchHandler *SwitchHandler::instance = nullptr;

SwitchHandler *SwitchHandler::get_instance()
{
    return SwitchHandler::instance;
}
SwitchHandler *SwitchHandler::get_instance(const  gpio_num_t *_relay_pins, const gpio_num_t *_button_pins, const gpio_num_t *_button_leds, size_t _pin_num)
{
    if(SwitchHandler::instance == nullptr)
    {
        SwitchHandler::instance = new SwitchHandler(_relay_pins, _button_pins, _button_leds, _pin_num);
    }
    return SwitchHandler::instance;
}

SwitchHandler::SwitchHandler(const  gpio_num_t *_relay_pins, const gpio_num_t *_button_pins, const gpio_num_t *_button_leds, size_t _pin_num)
: relay_pins(_relay_pins), button_pins(_button_pins), button_leds(_button_leds), pin_num(_pin_num),  s_handler(SettingsHandler::get_instance())
{ //Beware, GPIO34-39 can only be used as input and got not pull-up/down.
  //We should primarily use this for ADC input.
  //See https://esp-idf.readthedocs.io/en/latest/api/peripherals/gpio.html for GPIO info
  //There needs to be at least pin_num number of pins in each of the given pin pointers
  this->setup_button_pins();
  this->setup_button_leds();
  this->setup_relay_pins();
}
void SwitchHandler::setup_relay_pins()
{
    //Set the pins as output, and read the settings from the settingshandler
    //If no value is set, set default as off.

    char *switch_str = (char *)malloc(10);
    for(uint8_t i = 0; i < this->pin_num; i++)
    {
        ESP_ERROR_CHECK(gpio_set_direction(this->relay_pins[i], GPIO_MODE_OUTPUT));
        //Set default value if not already set
        snprintf(switch_str, 10,  "SWITCH%d", i);
        switch_state state = off;
        ESP_ERROR_CHECK( this->s_handler->set_default_value(switch_str, (uint8_t)state) );

        //Read the saved state
        ESP_ERROR_CHECK( this->s_handler->nvs_get(switch_str, (uint8_t *)&state) );
        //Set the state
        this->set_switch_state(i, state, false);
    }
    free(switch_str);

}

void SwitchHandler::setup_button_pins()
{ //Setup all the button pins. They needs to be set as input with pull-down

    for(uint8_t i = 0; i < this->pin_num; i++)
    {
        ESP_ERROR_CHECK(gpio_set_direction(this->relay_pins[i], GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_set_pull_mode(this->button_pins[i], GPIO_PULLDOWN_ONLY));
    }

}

void SwitchHandler::setup_button_leds()
{   //Setup pins as output. Their actual state will be set while calling setup_relay_pins()
    //This also means that this function must be called before setup_relay_pins().
    for(uint8_t i = 0; i < this->pin_num; i++)
    {
        ESP_ERROR_CHECK(gpio_set_direction(this->relay_pins[i], GPIO_MODE_OUTPUT));
    }
}

void SwitchHandler::set_switch_state(uint8_t switch_num, switch_state state, bool write_to_nvs)
{
    if(switch_num >= pin_num)
        return; //Simply ignore if not within range.

    //Set the relay state
    ESP_ERROR_CHECK(gpio_set_level(this->relay_pins[switch_num], state));
    //Set the led state
    ESP_ERROR_CHECK(gpio_set_level(this->button_leds[switch_num], state));
    //Write to nvs if requested
    if(write_to_nvs)
    {
        char *switch_str = (char *)malloc(10);
        snprintf(switch_str, 10,  "SWITCH%d", switch_num);
        ESP_ERROR_CHECK( this->s_handler->nvs_set(switch_str, (uint8_t)state) );
        free(switch_str);
    }
}
switch_state SwitchHandler::get_switch_state(uint8_t switch_num)
{   //Read directly on the pin to obtain the state.
    //TODO: Confirm that this is actually valid when the pin is configured as an output.
    if(switch_num >= pin_num)
        return invalid; //Simply ignore if not within range.

    return (switch_state)gpio_get_level(this->relay_pins[switch_num]);
}

uint8_t SwitchHandler::get_switch_count()
{
    return this->pin_num;
}
