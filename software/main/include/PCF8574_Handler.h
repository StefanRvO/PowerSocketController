#pragma once
//Handles communication with the PCF8574
#include "driver/gpio.h"
#include <list>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

namespace PCF8574_enum
{
    enum PCF8574_pin_state
    {
        LOW = 0,
        HIGH = 1,
    };
};

struct PCF8574_pin
{
    uint8_t address;
    uint8_t pin_num;
};

struct PCF8574
{
    uint8_t address;
    gpio_num_t interrupt_pin;
};

struct PCF8574_State
{
    PCF8574 device;
    //Defines which pins are input and which are output.
    uint8_t pin_types = 0xFF; //0 = output, 1 = input
    //Defines if the pins which are set to output should be high or low
    uint8_t output_state = 0;
};

struct PCF8574InterruptNotice
{
    uint8_t address;
    void *user_data;
    void (*callback)(uint8_t, void *);
};

class PCF8574_Handler;
class PCF8574_Handler
{
    public:
        //The two below functions registers a number of pins as input or output.
        int set_as_output(const PCF8574_pin *pins, const PCF8574_enum::PCF8574_pin_state* states, uint8_t pin_count); //Can only be used with pins all from the same device.
        int set_as_input(const PCF8574_pin *pins, uint8_t pin_count); //Can only be used with pins all from the same device.

        int set_output_state(const PCF8574_pin *pins, const PCF8574_enum::PCF8574_pin_state* states, uint8_t pin_count); //Can only be used with pins all from the same device.
        int read_input_state(const PCF8574_pin *pins, PCF8574_enum::PCF8574_pin_state* states, uint8_t pin_count); //Can only be used with pins all from the same device. Will block while reading.
        int set_output_state(const PCF8574_pin pin, const PCF8574_enum::PCF8574_pin_state state);
        int read_input_state(const PCF8574_pin pin, PCF8574_enum::PCF8574_pin_state *state);

        /*This function can be used to register an interrupt notice when the PCF8574's interrupt pin is asserted
        The registered callback function will be called from an interrupt context, so it is very important to return from
        it fast, and don't do anything which is illegal from inside an interrupt.
        */
        int register_interrupt_notice(uint8_t address, void (*callback)(uint8_t, void *), void *user_data);

        static PCF8574_Handler *get_instance();
        static PCF8574_Handler *get_instance(gpio_num_t scl, gpio_num_t sda, PCF8574 *devices, uint8_t device_count);
    private:
        xSemaphoreHandle i2c_mux;
        static PCF8574_State *devices;
        static uint8_t device_count;
        static std::list<PCF8574InterruptNotice> interrupt_notices;
        static PCF8574_Handler *instance;
        static void interrupt_handler(void *arg);

        PCF8574_State *find_device(uint8_t address);
        int read_state(PCF8574_State *device, uint8_t *state); //read the state from the device and put it into state.
                                                           //Only the bits set as input will be valid.
        int write_state(PCF8574_State *device); //Write the state to the device.
        PCF8574_Handler(gpio_num_t scl, gpio_num_t sda, PCF8574 *_devices, uint8_t device_count);

};
