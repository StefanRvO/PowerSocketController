#include "SwitchHandler.h"
#include <limits>
extern "C"
{
    #include "stdlib.h"
    #include "driver/ledc.h"
    #include "esp_system.h"
    #include "esp_err.h"
}
using namespace PCF8574_enum;

__attribute__((unused)) static const char *TAG = "SwitchHandler";

SwitchHandler *SwitchHandler::instance = nullptr;

PCF8574_pin_state SwitchHandler::switch_state_to_PCF8574(switch_state state)
{
    if(this->relay_on_level == true)
    {
        if(state == on) return HIGH;
        else            return LOW;
    }
    else
    {
        if(state == on) return LOW;
        else            return HIGH;
    }
}
SwitchHandler *SwitchHandler::get_instance()
{
    assert(SwitchHandler::instance != nullptr);
    return SwitchHandler::instance;
}

SwitchHandler *SwitchHandler::get_instance(const  PCF8574_pin *_relay_pins, const PCF8574_pin *_relay_voltage_pin,
        const PCF8574_pin *_button_pins, const gpio_num_t *_button_leds,
         size_t _pin_num, bool _relay_on_level)
{
    if(SwitchHandler::instance == nullptr)
    {
        SwitchHandler::instance = new SwitchHandler(_relay_pins, _relay_voltage_pin, _button_pins, _button_leds, _pin_num, _relay_on_level);
    }
    return SwitchHandler::instance;
}

SwitchHandler::SwitchHandler(const  PCF8574_pin *_relay_pins, const PCF8574_pin *_relay_voltage_pin,
        const PCF8574_pin *_button_pins, const gpio_num_t *_button_leds,
         size_t _pin_num, bool _relay_on_level)
: relay_pins(_relay_pins), relay_voltage_pin(_relay_voltage_pin),
    button_pins(_button_pins), button_leds(_button_leds),
    pin_num(_pin_num),  s_handler(SettingsHandler::get_instance()),
    pcf8574(PCF8574_Handler::get_instance()), set_mux(xSemaphoreCreateMutex()),
    led_settings_lock(xSemaphoreCreateMutex()),
    relay_on_level(_relay_on_level)
{ //Beware, GPIO34-39 can only be used as input and got not pull-up/down.
  //We should primarily use this for ADC input.
  //See https://esp-idf.readthedocs.io/en/latest/api/peripherals/gpio.html for GPIO info
  //There needs to be at least pin_num number of pins in each of the given pin pointers (except relay_voltage_pin)
  this->state_buff = (switch_state *)malloc(this->pin_num * sizeof(switch_state));
  this->button_states = (button_state *)malloc(this->pin_num * sizeof(button_state));
  this->led_state_buff = (switch_state *)malloc(this->pin_num * sizeof(switch_state));

  this->setup_button_pins();
  this->setup_button_leds();
  this->setup_relay_pins();
  for(uint8_t i = 0; i < this->pin_num; i++)
  {
      this->led_state_buff[i] = off;
      button_state &state = this->button_states[i];
      state.state = idle;
      state.raw_state = false;
      state.filtered_state = false;
      state.timer = 0;
  }
  if(this->led_settings_lock == NULL) printf("error creating switchhandler led semaphore!\n");

  this->button_poll_timer = xTimerCreate ("btn_poll_tmr", POLL_TIME / portTICK_PERIOD_MS, pdTRUE, 0, SwitchHandler::poll_buttons);
  xTimerStart(this->button_poll_timer, 100000 / portTICK_PERIOD_MS);
}

SwitchHandler::~SwitchHandler()
{
    free(this->state_buff);
    free(this->button_states);
    free(this->led_state_buff);

}
void SwitchHandler::setup_relay_pins()
{
    //Set the pins as output, and read the settings from the settingshandler
    //If no value is set, set default as off.

    PCF8574_pin_state *states = new PCF8574_pin_state[this->pin_num];
    PCF8574_pin_state relay_voltage_pin_state = HIGH;
    char switch_str[10];

    for(uint8_t i = 0; i < this->pin_num; i++)
    {
        //Set default value if not already set
        snprintf(switch_str, sizeof(switch_str),  "SWITCH%d", i);
        switch_state state = off;
        ESP_ERROR_CHECK( this->s_handler->set_default_value(switch_str, (uint8_t)state) );
        //Read the saved state
        ESP_ERROR_CHECK( this->s_handler->nvs_get(switch_str, (uint8_t *)&state) );
        //Set the state
        this->state_buff[i] = state;
        states[i] = this->switch_state_to_PCF8574(state);
    }
    delete[] states;
    this->pcf8574->set_as_output(this->relay_voltage_pin, &relay_voltage_pin_state, 1);
    this->pcf8574->set_as_output(this->relay_pins, states, this->pin_num);
}

void SwitchHandler::setup_button_pins()
{ //Setup all the button pins.
    this->pcf8574->set_as_input(this->button_pins, this->pin_num);
}

void SwitchHandler::setup_button_leds()
{
    //We use the LEDC driver for controlling the LED's.
    //This will make fancy stuff and animations possible..
    //Setup the timer
    ledc_timer_config_t ledc_timer;
    ledc_timer.bit_num = LEDC_BITWIDTH;             //set timer counter bit number
    ledc_timer.freq_hz = 5000;                      //set frequency of pwm
    ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;   //timer mode,
    ledc_timer.timer_num = LEDC_TIMER_0;            //timer index
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    //Setup the channel
    for(uint8_t i = 0; i < this->pin_num; i++)
    {
        ledc_channel_config_t ledc_channel;
        ledc_channel.channel = (ledc_channel_t)i;
        ledc_channel.duty = 0; //Off.
        ledc_channel.gpio_num = this->button_leds[i];
        ledc_channel.intr_type = LEDC_INTR_FADE_END;
        ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
        ledc_channel.timer_sel = LEDC_TIMER_0;
        //set the configuration
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }
    ESP_ERROR_CHECK(ledc_fade_func_install(0));

}

void SwitchHandler::set_switch_state(uint8_t switch_num, switch_state state, bool write_to_nvs)
{
    if(switch_num >= this->pin_num)
        return; //Simply ignore if not within range.
    xSemaphoreTake(this->set_mux, portMAX_DELAY);

    if(this->relay_voltage_pin_timeout == 0)
    {
        PCF8574_pin_state relay_voltage_pin_state = HIGH;
        this->pcf8574->set_output_state(this->relay_voltage_pin, &relay_voltage_pin_state, 1);
    }
    this->relay_voltage_pin_timeout = 200;

    //Set the relay state
    PCF8574_pin_state relay_state = this->switch_state_to_PCF8574(state);
    this->pcf8574->set_output_state(this->relay_pins + switch_num, &relay_state, 1);
    //Set the state in the buffer
    this->state_buff[switch_num] = state;
    //Write to nvs if requested
    if(write_to_nvs)
    {
        char switch_str[10];
        snprintf(switch_str, 10,  "SWITCH%d", switch_num);
        ESP_ERROR_CHECK( this->s_handler->nvs_set(switch_str, (uint8_t)state) );
    }
    xSemaphoreGive(this->set_mux);

}
switch_state SwitchHandler::get_switch_state(uint8_t switch_num)
{   //Return the value stored in the buffer. this also means that we need to make absolutely sure that all changes of the pin
    //Is reflected in the buffer.

    if(switch_num >= pin_num)
        return invalid; //Simply ignore if not within range.

    return this->state_buff[switch_num];
}

uint8_t SwitchHandler::get_switch_count()
{
    return this->pin_num;
}

void SwitchHandler::poll_buttons(TimerHandle_t xTimer)
{
    //Pool all the buttons and update their states
    xSemaphoreTake(SwitchHandler::instance->set_mux, portMAX_DELAY);
    if(SwitchHandler::instance->relay_voltage_pin_timeout && --SwitchHandler::instance->relay_voltage_pin_timeout == 0)
    {
        PCF8574_pin_state relay_voltage_pin_state = LOW;
        SwitchHandler::instance->pcf8574->set_output_state(SwitchHandler::instance->relay_voltage_pin, &relay_voltage_pin_state, 1);
    }
    xSemaphoreGive(SwitchHandler::instance->set_mux);
    PCF8574_pin_state *states = new PCF8574_pin_state[SwitchHandler::instance->pin_num];
    SwitchHandler::instance->pcf8574->read_input_state(SwitchHandler::instance->button_pins, states, SwitchHandler::instance->pin_num); //Can only be used with pins all from the same device. Will block while reading.
    for(uint8_t i = 0; i < SwitchHandler::instance->pin_num; i++)
    {
        button_event emitted_event = SwitchHandler::instance->poll_button(i, states[i]);
        SwitchHandler::instance->handle_event(emitted_event, i);
    }
    delete[] states;
    SwitchHandler::instance->handle_button_states();
    SwitchHandler::instance->handle_leds();
}


void SwitchHandler::handle_event(button_event the_event, uint8_t button_num)
{   //Handles events triggered by statechanges of the buttons.
    switch(the_event)
    {
        case no_event:
            return;
        case single_push:
            //toggle the switch
            this->set_switch_state(button_num, (switch_state)!this->get_switch_state(button_num));
            return;
        case single_release:
            return;
        case double_push:
            this->set_switch_state(button_num, (switch_state)!this->get_switch_state(button_num));
            //Just handle double click as single click for now.
            return;
        case double_release:
            return;
        default:
            return;
    }
    return;
}

void SwitchHandler::handle_button_states()
{
    //Handles actions which is to be taken as a result of the current state of the
    //buttons, as well as a combination of these states.

    //Count how many buttons are pressed.
    uint8_t btn_count = 0;
    uint64_t min_time = std::numeric_limits<uint64_t>::max();
    for(uint8_t i = 0; i < this->pin_num; i++)
    {
        button_state & state = this->button_states[i];
        if(state.filtered_state == true)
        {
            btn_count += 1;
            if(state.timer <= min_time)
                min_time = state.timer;
        }
    }
    //printf("min_time: %llu\n", min_time);
    /*if(min_time > 5000 and min_time != std::numeric_limits<uint64_t>::max())
    {
        //we change into blinking state, with a timout of 500 ms
        //And a delay between blinks of (20000 - min_time) / 20
        this->set_led_mode(blinking, 500);
        if(min_time < 14000)
        {
            this->set_led_blink_time( (15000 - min_time) / 20);
        }
        else
        {
            this->s_handler->reset_settings();
            esp_restart();
        }
    }*/
    return;
}
void SwitchHandler::handle_leds()
{
    //Modify the mode and timeout, and make a copy of the mode.
    xSemaphoreTake(this->led_settings_lock, 100000 / portTICK_RATE_MS);
    if(this->led_state_timeout <= POLL_TIME) this->led_state_timeout = 0;
    else this->led_state_timeout -= POLL_TIME;
    if(this->led_state_timeout == 0) this->led_mode = output;
    led_control_mode led_mode_copy = led_mode;
    xSemaphoreGive(this->led_settings_lock);
    //Set the leds depending on the mode

    switch(led_mode_copy)
    {
        case output:
            //set all the leds to the switch state
            for(uint8_t i = 0; i < this->pin_num; i++)
            {
                if(this->led_state_buff[i] != this->state_buff[i])
                {
                    if(this->state_buff[i] == off)
                    {
                        ESP_ERROR_CHECK(ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i, 0, FADE_TIME));
                        ESP_ERROR_CHECK(ledc_fade_start(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i, LEDC_FADE_NO_WAIT));
                    //    ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i, 0));
                    //    ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i));

                    }
                    else
                    {
                        ESP_ERROR_CHECK(ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i, (1 << LEDC_BITWIDTH) - 1, FADE_TIME));
                        ESP_ERROR_CHECK(ledc_fade_start(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i, LEDC_FADE_NO_WAIT));
                    //    ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i, 1000));
                    //    ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i));

                    }
                    this->led_state_buff[i] = this->state_buff[i];
                }
            }
            break;
        case blinking:
            this->blink_counter += POLL_TIME;
            if(this->blink_counter >= blink_time)
            {
                this->blink_counter = 0;
                //Set to the reverse of the first led
                switch_state new_state = (switch_state)!this->led_state_buff[0];
                for(uint8_t i = 0; i < this->pin_num; i++)
                {
                    if(new_state == off)
                    {
                        ESP_ERROR_CHECK(ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i, 0, blink_time / 4));
                        ESP_ERROR_CHECK(ledc_fade_start(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i, LEDC_FADE_NO_WAIT));
                    }
                    else
                    {
                        ESP_ERROR_CHECK(ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i, (1 << LEDC_BITWIDTH) - 1, blink_time / 4));
                        ESP_ERROR_CHECK(ledc_fade_start(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)i, LEDC_FADE_NO_WAIT));
                    }
                    this->led_state_buff[i] = new_state;
                }
            }
            break;
        default:
            break;
    }
    return;
}

void SwitchHandler::set_led_mode(led_control_mode mode, uint64_t timeout)
{
    xSemaphoreTake(this->led_settings_lock, 100000 / portTICK_RATE_MS);
    this->led_mode = mode;
    this->led_state_timeout = timeout;
    xSemaphoreGive(this->led_settings_lock);
}

void SwitchHandler::set_led_blink_time(uint64_t blink_time_)
{
    xSemaphoreTake(this->led_settings_lock, 100000 / portTICK_RATE_MS);
    this->blink_time = blink_time_;
    xSemaphoreGive(this->led_settings_lock);
}

button_event SwitchHandler::poll_button(uint8_t button_num, bool raw_state)
{
    //Debouncing
    button_state &state = this->button_states[button_num];
    if(state.raw_state == raw_state)
    {
        state.timer += POLL_TIME;
        if(state.timer  >= DEBOUNCE_TIME)
            state.filtered_state = state.raw_state;
    }
    else state.timer = 0;

    state.raw_state = raw_state;
    //printf("State <Button: %d, raw: %d, debounce: %d>\n", button_num, raw_state,  state.filtered_state);

    switch(state.state)
    {
        case idle:
            if(state.filtered_state == true)
            {
                state.state = first_push;
                return button_event::single_push;
            }
            break;
        case first_push:
            if(state.filtered_state == false)
            {
                state.state = first_release;
                return button_event::single_release;
            }
            break;
        case first_release:
            if(state.filtered_state == false and state.timer >= DOUBLECLICK_THRESHOLD)
                state.state = idle;
            else if(state.filtered_state == true)
            {
                state.state = second_push;
                return button_event::double_push;
            }
            break;
        case second_push:
            if(state.filtered_state == false)
            {
                state.state = second_release;
                return button_event::double_release;
            }
            break;
        case second_release:
            state.state = idle;
            break;
        default:
            assert(false);
            break;
    }
    return button_event::no_event;
}
