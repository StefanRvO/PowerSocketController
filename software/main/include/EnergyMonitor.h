#pragma once
#include "CS5463.h"
#include <vector>
#include <PCF8574_Handler.h>
class EnergyMonitor;
class EnergyMonitor
{
    public:
        static EnergyMonitor *get_instance();
        static EnergyMonitor *get_instance(gpio_num_t *slave_selects, PCF8574_pin *reset_pin, uint8_t device_count);
        static EnergyMonitor *instance;
    private:
        EnergyMonitor(gpio_num_t *slave_selects, PCF8574_pin *reset_pin, uint8_t device_count);
        std::vector<CS5463> devices;
        PCF8574_Handler *pcf8574;
        PCF8574_pin *reset_pin;
        static void energy_monitor_thread_wrapper(void *user);
        void energy_monitor_thread();
        void print_status_register();
        void cs5463_setup();
};
