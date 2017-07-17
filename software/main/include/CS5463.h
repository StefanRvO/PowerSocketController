#pragma once
//This class handles the communication and setup of a single CS5463 Energy IC.
//The class will not be threadsafe, don't use functionallity from it from multiple threads simultaneously.
#include "esp_system.h"
#include "driver/gpio.h"
#include <cstdint>
#include "driver/spi_master.h"

class CS5463
{
    public:
        CS5463(gpio_num_t slave_select);
        static void set_spi_pins(gpio_num_t miso, gpio_num_t mosi, gpio_num_t clk);
    private:
        static spi_device_handle_t spi;
        static uint8_t device_cnt;
        static bool initialised;
        static gpio_num_t miso_pin;
        static gpio_num_t mosi_pin;
        static gpio_num_t clk_pin;
};
