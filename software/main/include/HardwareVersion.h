#pragma once
//Class to hold and read information about which hardware version the software is currently running on
#include <cstdint>
#include "driver/gpio.h"

enum pin_state
{
    LOW,
    HIGH,
    floating,
};

struct hardware_version_t
{
    uint8_t gpio_version; //Version determined by GPIO pins
    uint8_t external_version; //Version determined by external hardware (i.e. I2C devices)
};

class HardwareVersion;

class HardwareVersion
{
    public:
        static hardware_version_t hardware_version;
        static hardware_version_t get_hardware_version();
        static void read_hardware_version();
    private:
        static pin_state read_pin_state(gpio_num_t pin);
        static uint8_t read_external_version();
        static uint8_t read_gpio_version();
};
